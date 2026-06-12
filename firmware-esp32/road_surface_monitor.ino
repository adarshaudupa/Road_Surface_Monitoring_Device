#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ── BLE type aliases ────────────────────────────────────────────────────────
#define BLEDEVICE                   BLEDevice
#define BLEServerT                  BLEServer
#define BLEServiceT                 BLEService
#define BLECharacteristicT          BLECharacteristic
#define BLEServerCallbacksT         BLEServerCallbacks
#define BLECharacteristicCallbacksT BLECharacteristicCallbacks
#define BLEAdvertisingT             BLEAdvertising
#define PROP_READ                   BLECharacteristic::PROPERTY_READ
#define PROP_WRITE                  BLECharacteristic::PROPERTY_WRITE
#define PROP_WRITE_NR               BLECharacteristic::PROPERTY_WRITE_NR
#define PROP_NOTIFY                 BLECharacteristic::PROPERTY_NOTIFY
#define ADD_CCCD(c)                 (c)->addDescriptor(new BLE2902())

// ── PIN DEFINITIONS (ESP32-S3 SAFE ZONE) ─────────────────────────────────────
const int PIN_PWMA = 1;
const int PIN_PWMB = 2;
const int PIN_AIN1 = 4;
const int PIN_AIN2 = 5;
const int PIN_BIN1 = 12;
const int PIN_BIN2 = 13;
const int PIN_STBY = 14;

// ── BLE UUIDs ────────────────────────────────────────────────────────────────
static const char* BLE_NAME     = "RC_CAR_BLE";
static const char* SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

// ── WiFi / telemetry config ─────────────────────────────────────────────────
const char* WIFI_SSID = "Nothing Phone 2a";
const char* WIFI_PASS = "1234567890";
const char* SERVER_URL = "http://10.193.26.79:8000/api/ingest";
const char* DEVICE_ID = "esp32-car-01";

// ── UART from STM32 ──────────────────────────────────────────────────────────
static const int STM32_RX_PIN = 16;
static const int STM32_TX_PIN = 17;

// Legacy packet support. Keep this if older firmware still sends TIME/LAT/LON.
const double GPS_SCALE = 1000000.0;
const float LSM6DS3_LSB_PER_G = 16384.0f;

// Distance baseline for ultrasonic depth/height calculation.
const float DISTANCE_BASELINE_ALPHA = 0.05f;

// ── PWM & Speed settings ─────────────────────────────────────────────────────
const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int MAX_SPEED = 255;

// ── State ────────────────────────────────────────────────────────────────────
volatile char lastDir = 'S';
volatile unsigned long lastCmdMs = 0;
volatile bool deviceConnected = false;
volatile bool isAwake = false;

BLECharacteristicT* txChar = nullptr;
HardwareSerial STM32_UART(2);

String uartBuffer;
float distanceBaselineCm = 0.0f;
bool baselineReady = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastUartIdleLog = 0;
wl_status_t lastWiFiStatus = WL_IDLE_STATUS;
bool wifiStarted = false;

void resetDistanceBaseline() {
  distanceBaselineCm = 0.0f;
  baselineReady = false;
}

float updateBaseline(float distanceCm) {
  if (!baselineReady) {
    distanceBaselineCm = distanceCm;
    baselineReady = true;
    return 0.0f;
  }

  distanceBaselineCm =
      distanceBaselineCm * (1.0f - DISTANCE_BASELINE_ALPHA) +
      distanceCm * DISTANCE_BASELINE_ALPHA;

  return distanceCm - distanceBaselineCm;
}

void logWiFiStatus() {
  wl_status_t status = WiFi.status();
  if (status == lastWiFiStatus) {
    return;
  }

  lastWiFiStatus = status;

  switch (status) {
    case WL_CONNECTED:
      Serial.println("WiFi connected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("WiFi SSID not found");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("WiFi connect failed");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("WiFi connection lost");
      break;
    case WL_DISCONNECTED:
      Serial.println("WiFi disconnected");
      break;
    default:
      Serial.println("WiFi status changed");
      break;
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWiFiAttempt < 5000) {
    return;
  }

  lastWiFiAttempt = now;
  if (!wifiStarted) {
    Serial.println("Connecting to WiFi...");
    wifiStarted = true;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void pwmSetup() {
  ledcAttach(PIN_PWMA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_PWMB, PWM_FREQ, PWM_RES);
}

void pwmWrite(int pin, int duty) {
  ledcWrite(pin, duty);
}

void setMotor(int in1, int in2, int pwmPin, int speed, int dir) {
  if (dir == 0 || speed <= 0 || !isAwake) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    pwmWrite(pwmPin, 0);
    return;
  }

  digitalWrite(in1, dir > 0 ? HIGH : LOW);
  digitalWrite(in2, dir > 0 ? LOW : HIGH);
  pwmWrite(pwmPin, constrain(speed, 0, MAX_SPEED));
}

void stopAll() {
  setMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, 0, 0);
  setMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, 0, 0);
}

void drive(char cmd) {
  if (!isAwake) {
    return;
  }

  switch (cmd) {
    case 'F':
      setMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, MAX_SPEED, 1);
      setMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, MAX_SPEED, 1);
      break;
    case 'B':
      setMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, MAX_SPEED, -1);
      setMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, MAX_SPEED, -1);
      break;
    case 'L':
      setMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, MAX_SPEED, -1);
      setMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, MAX_SPEED, 1);
      break;
    case 'R':
      setMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, MAX_SPEED, 1);
      setMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, MAX_SPEED, -1);
      break;
    default:
      stopAll();
      break;
  }
}

void sendStatus(const char* msg) {
  if (txChar && deviceConnected) {
    txChar->setValue((uint8_t*)msg, strlen(msg));
    txChar->notify();
    Serial.print("[TX] ");
    Serial.println(msg);
  }
}

void handleCommand(String raw) {
  raw.trim();
  raw.toUpperCase();
  if (raw.length() == 0) {
    return;
  }

  Serial.print("[RX] ");
  Serial.println(raw);
  char ch = raw[0];

  if (ch == 'W') {
    isAwake = true;
    digitalWrite(PIN_STBY, HIGH);
    sendStatus("SYSTEM:AWAKE");
    return;
  }

  if (ch == 'X') {
    isAwake = false;
    digitalWrite(PIN_STBY, LOW);
    stopAll();
    lastDir = 'S';
    sendStatus("SYSTEM:SLEEP");
    return;
  }

  if (ch == 'C') {
    resetDistanceBaseline();
    sendStatus("SENSOR:CALIBRATED");
    return;
  }

  if (!isAwake) {
    sendStatus("ERR:SLEEPING_SEND_W");
    return;
  }

  if (ch != 'F' && ch != 'B' && ch != 'L' && ch != 'R' && ch != 'S') {
    sendStatus("ERR:UNKNOWN_CMD");
    return;
  }

  lastCmdMs = millis();
  lastDir = ch;
  drive(lastDir);

  char buf[16];
  snprintf(buf, sizeof(buf), "DIR:%c", lastDir);
  sendStatus(buf);
}

class ServerCallbacks : public BLEServerCallbacksT {
public:
  void onConnect(BLEServerT* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Connected");
  }

  void onDisconnect(BLEServerT* pServer) override {
    deviceConnected = false;
    lastDir = 'S';
    stopAll();
    Serial.println("[BLE] Disconnected");
    delay(500);
    BLEDEVICE::startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacksT {
public:
  void onWrite(BLECharacteristicT* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value.length() > 0) {
      handleCommand(value);
    }
  }
};

int splitCsv(const String& line, String parts[], int maxParts) {
  int count = 0;
  int start = 0;

  while (count < maxParts) {
    int comma = line.indexOf(',', start);
    if (comma == -1) {
      parts[count++] = line.substring(start);
      break;
    }

    parts[count++] = line.substring(start, comma);
    start = comma + 1;
  }

  for (int i = 0; i < count; ++i) {
    parts[i].trim();
  }

  return count;
}

bool parseLegacyTelemetry(const String& line,
                          float& intensityG,
                          float& lat,
                          float& lng,
                          bool& hasUltrasonic,
                          float& ultrasonicDeltaCm) {
  long latRaw = 0;
  long lngRaw = 0;
  long distanceCm = 0;
  long accelZRaw = 0;
  char timeBuffer[16] = {0};

  int matched = sscanf(
      line.c_str(),
      "TIME:%15[^,],LAT:%ld,LON:%ld,DIST:%ld,ACCZ:%ld",
      timeBuffer,
      &latRaw,
      &lngRaw,
      &distanceCm,
      &accelZRaw);

  if (matched != 5) {
    return false;
  }

  lat = static_cast<float>(latRaw / GPS_SCALE);
  lng = static_cast<float>(lngRaw / GPS_SCALE);
  intensityG = fabs(static_cast<float>(accelZRaw)) / LSM6DS3_LSB_PER_G;
  ultrasonicDeltaCm = updateBaseline(static_cast<float>(distanceCm));
  hasUltrasonic = true;
  return true;
}

static const int TELEMETRY_INVALID = 0;
static const int TELEMETRY_NO_EVENT = 1;
static const int TELEMETRY_EVENT = 2;

int parseCsvTelemetry(const String& line,
                      String& eventTypeName,
                      float& intensityG,
                      float& lat,
                      float& lng,
                      bool& hasUltrasonic,
                      float& ultrasonicDeltaCm) {
  String parts[8];
  int count = splitCsv(line, parts, 8);
  if (count < 2) {
    return TELEMETRY_INVALID;
  }

  String kind = parts[0];
  kind.toUpperCase();

  if (kind == "D") {
    float distanceCm = parts[1].toFloat();
    updateBaseline(distanceCm);
    return TELEMETRY_NO_EVENT;
  }

  if (kind != "P" && kind != "R" && kind != "B") {
    return TELEMETRY_INVALID;
  }

  float shockValue = parts[1].toFloat();
  intensityG = fabs(shockValue) / LSM6DS3_LSB_PER_G;

  if (count >= 6) {
    float distanceCm = parts[2].toFloat();
    lat = parts[3].toFloat();
    lng = parts[4].toFloat();
    ultrasonicDeltaCm = updateBaseline(distanceCm);
    hasUltrasonic = true;
  } else if (count >= 5) {
    lat = parts[2].toFloat();
    lng = parts[3].toFloat();
  } else {
    return TELEMETRY_INVALID;
  }

  if (kind == "P") {
    eventTypeName = "pothole";
  } else if (kind == "B") {
    eventTypeName = "speed_bump";
  } else {
    eventTypeName = "rough_patch";
  }

  return TELEMETRY_EVENT;
}

// ── NEW: Added rawLine parameter to push raw UART to the backend
void sendTelemetry(const String& eventTypeName,
                   float lat,
                   float lng,
                   float intensityG,
                   bool hasUltrasonic,
                   float ultrasonicDeltaCm,
                   const String& rawLine) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi not connected");
    return;
  }

  WiFiClient wifiClient;
  HTTPClient http;
  if (!http.begin(wifiClient, SERVER_URL)) {
    Serial.println("[HTTP] begin failed");
    return;
  }

  http.setTimeout(3000);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  if (eventTypeName.length() > 0) {
    payload += "\"event_type\":\"";
    payload += eventTypeName;
    payload += "\",";
  }

  payload += "\"device_id\":\"";
  payload += DEVICE_ID;
  payload += "\",";
  payload += "\"lat\":" + String(lat, 6) + ",";
  payload += "\"lng\":" + String(lng, 6) + ",";
  payload += "\"accel_peak_g\":" + String(intensityG, 2);

  if (hasUltrasonic) {
    payload += ",\"ultrasonic_delta_cm\":" + String(ultrasonicDeltaCm, 2);
  }

  // --- ADD THE RAW UART DATA TO THE JSON PAYLOAD ---
  payload += ",\"raw_data\":\"" + rawLine + "\"";

  payload += "}";

  Serial.print("[HTTP] Sending payload: ");
  Serial.println(payload);

  int status = http.POST(payload);
  String response = http.getString();
  http.end();

  Serial.print("[HTTP] status: ");
  Serial.println(status);
  Serial.println(response);
}

void handleTelemetryLine(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  Serial.print("[UART] ");
  Serial.println(line);

  float intensityG = 0.0f;
  float lat = 0.0f;
  float lng = 0.0f;
  bool hasUltrasonic = false;
  float ultrasonicDeltaCm = 0.0f;
  String eventTypeName;

  if (line.startsWith("TIME:")) {
    if (parseLegacyTelemetry(line, intensityG, lat, lng, hasUltrasonic, ultrasonicDeltaCm)) {
      // Pass the raw 'line' into sendTelemetry
      sendTelemetry(eventTypeName, lat, lng, intensityG, hasUltrasonic, ultrasonicDeltaCm, line);
    } else {
      Serial.println("[UART] Invalid legacy packet");
    }
    return;
  }

  int result = parseCsvTelemetry(
      line,
      eventTypeName,
      intensityG,
      lat,
      lng,
      hasUltrasonic,
      ultrasonicDeltaCm);

  if (result == TELEMETRY_EVENT) {
    // Pass the raw 'line' into sendTelemetry
    sendTelemetry(eventTypeName, lat, lng, intensityG, hasUltrasonic, ultrasonicDeltaCm, line);
  } else if (result == TELEMETRY_INVALID) {
    Serial.println("[UART] Invalid packet");
  }
}

void pollTelemetrySerial() {
  while (STM32_UART.available()) {
    char c = static_cast<char>(STM32_UART.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      String line = uartBuffer;
      uartBuffer = "";
      handleTelemetryLine(line);
      continue;
    }

    if (uartBuffer.length() < 180) {
      uartBuffer += c;
    } else {
      uartBuffer = "";
      Serial.println("[UART] Buffer overflow");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);

  pwmSetup();

  isAwake = false;
  digitalWrite(PIN_STBY, LOW);
  stopAll();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  STM32_UART.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  BLEDEVICE::init(BLE_NAME);
  BLEServerT* server = BLEDEVICE::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEServiceT* service = server->createService(SERVICE_UUID);

  BLECharacteristicT* rxChar = service->createCharacteristic(RX_UUID, PROP_WRITE | PROP_WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  txChar = service->createCharacteristic(TX_UUID, PROP_READ | PROP_NOTIFY);
  ADD_CCCD(txChar);

  service->start();
  BLEDEVICE::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDEVICE::startAdvertising();

  Serial.println("[BOOT] Ready. Send 'W' to start the car, 'C' to calibrate the ultrasonic baseline.");
}

void loop() {
  connectWiFi();
  logWiFiStatus();
  pollTelemetrySerial();

  if (STM32_UART.available() == 0 && uartBuffer.length() == 0) {
    unsigned long now = millis();
    if (now - lastUartIdleLog > 5000) {
      Serial.println("Waiting for STM32 data...");
      lastUartIdleLog = now;
    }
  }
}