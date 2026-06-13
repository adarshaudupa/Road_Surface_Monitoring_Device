<div align="center">

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 1 — BANNER                                           ║
  ║  File: docs/images/banner.png                               ║
  ║  Suggested size: 1280 × 400 px                              ║
  ║  Use Canva / Figma to make a dark-themed banner with the    ║
  ║  project name, a road icon, and your institution name.       ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Banner](docs/images/banner.png)

#  Road Condition Monitoring System

**A vehicle-mounted embedded sensing platform that autonomously detects road anomalies,
geo-tags them with real-time GPS, and streams telemetry to a live web dashboard.**

<br>

[![STM32](https://img.shields.io/badge/MCU-STM32F446RE-03234B?style=for-the-badge&logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![ESP32](https://img.shields.io/badge/Wireless-ESP32--S3-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![C](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Python](https://img.shields.io/badge/Backend-Python-3776AB?style=for-the-badge&logo=python&logoColor=white)](https://python.org)
[![Arduino](https://img.shields.io/badge/ESP32_Firmware-Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://arduino.cc)

[![IMU](https://img.shields.io/badge/IMU-LSM6DS3-blueviolet?style=flat-square)](https://www.st.com/en/mems-and-sensors/lsm6ds3.html)
[![GPS](https://img.shields.io/badge/GPS-NEO--6M-brightgreen?style=flat-square)](https://www.u-blox.com/)
[![Ultrasonic](https://img.shields.io/badge/Ultrasonic-HC--SR04-orange?style=flat-square)](https://components101.com/sensors/ultrasound-sensor-hc-sr04)
[![Platform](https://img.shields.io/badge/Platform-RC_Vehicle-yellow?style=flat-square)]()
[![Status](https://img.shields.io/badge/Status-Functional_Prototype-4dd599?style=flat-square)]()

</div>

---

##  Table of Contents

- [Overview](#-overview)
- [Live Dashboard Preview](#-live-dashboard-preview)
- [System Architecture](#-system-architecture)
- [Hardware Platform](#-hardware-platform)
- [Sensor Subsystems](#-sensor-subsystems)
- [Firmware Architecture](#-firmware-architecture)
- [Communication Protocol](#-communication-protocol)
- [Road Event Classification](#-road-event-classification)
- [Web Dashboard & Backend](#-web-dashboard--backend)
- [Repository Structure](#-repository-structure)
- [Getting Started](#-getting-started)
- [Team](#-team)
- [Concepts Demonstrated](#-concepts-demonstrated)
- [Future Improvements](#-future-improvements)
- [Current Status](#-current-status)

---

##  Overview

The **Road Condition Monitoring System** is a complete end-to-end embedded IoT platform that mounts on an RC vehicle and autonomously surveys road surface quality in real time.

The STM32F446RE microcontroller fuses data from an **IMU**, **ultrasonic rangefinder**, and **GPS receiver** to detect and geo-tag road anomalies — potholes and rough patches. Classified events are forwarded over UART to an **ESP32-S3 Wi-Fi gateway**, which uploads them to a **Python backend** for live visualization on a Leaflet.js map dashboard.

<br>

| | Capability |
|:---:|:---|
|  | **Multi-sensor fusion** — IMU shock detection cross-validated with ultrasonic surface profiling |
|  | **GPS geo-tagging** — NMEA GPGGA parsing with interrupt-driven ring-buffer architecture |
|  | **Inter-MCU telemetry** — Structured ASCII packet protocol over STM32 ↔ ESP32 UART link |
|  | **Cloud uplink** — HTTP POST pipeline from ESP32 to Python REST API |
|  | **Live dashboard** — Color-coded map markers with per-event shock & distance metrics |
|  | **Vehicle integration** — 3D-printed sensor mounts on a custom RC chassis |

---

##  Live Dashboard Preview

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 2 — DASHBOARD SCREENSHOT                             ║
  ║  File: docs/images/dashboard.png                            ║
  ║  Suggested size: 1280 × 720 px                              ║
  ║  Take a full-page screenshot of the running dashboard with   ║
  ║  some events already plotted on the map.                     ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Dashboard](docs/images/dashboard.png)

> Real-time Leaflet.js map showing pothole (🔴) and rough road (🟠) events, with per-event shock magnitude, GPS coordinates, and timestamp.

---

##  System Architecture

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 3 — SYSTEM ARCHITECTURE DIAGRAM                      ║
  ║  File: docs/images/architecture.png                         ║
  ║  Suggested size: 1400 × 600 px                              ║
  ║  Use draw.io / Excalidraw to recreate the ASCII diagram      ║
  ║  below as a clean block diagram with icons for each node.    ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Architecture Diagram](docs/images/architecture.png)

```
  ┌──────────────┐  I²C    ┌───────────────────────────────────────────────────┐
  │  LSM6DS3 IMU │───────► │                                                   │
  └──────────────┘         │                                                   │  UART    ┌───────────┐  Wi-Fi   ┌────────────────┐
                           │            STM32F446RE                            │────────► │  ESP32-S3 │─────────►│ Backend Server │
  ┌──────────────┐  TIM5   │                                                   │          └───────────┘          └───────┬────────┘
  │   HC-SR04    │───────► │  • Sensor acquisition & fusion                    │                                         │
  └──────────────┘         │  • Event classification (pothole / rough road)    │                                         ▼
                           │  • GPS stream parsing (NMEA GPGGA)                │                                 ┌────────────────┐
  ┌──────────────┐  UART   │  • Telemetry packet generation                    │                                 │ Web Dashboard  │
  │   NEO-6M GPS │───────► │                                                   │                                 │  (Leaflet.js)  │
  └──────────────┘         └───────────────────────────────────────────────────┘                                 └────────────────┘
```

Data flows from raw sensor readings all the way to a browser-rendered map in under a second.

---

## 🔧 Hardware Platform

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 4 — HARDWARE / RC CAR PHOTO                           ║
  ║  File: docs/images/hardware.png                              ║
  ║  Suggested size: 1200 × 800 px                               ║
  ║  A well-lit photo of the RC car with components labelled.    ║
  ║  Use Canva or GIMP to add callout labels over the photo.     ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Hardware Platform](docs/images/hardware.png)

### Processing Nodes

| Component | Role |
|---|---|
| **STM32F446RE** | Primary sensing & processing node — sensor drivers, event classification, telemetry generation |
| **ESP32-S3** | Communication gateway — Wi-Fi HTTP uplink + BLE-based RC vehicle control |

### Sensors & Peripherals

| Device | Interface | Function |
|---|---|---|
| **LSM6DS3** | I²C | 6-axis IMU — accelerometer for shock detection |
| **HC-SR04** | TIM5 Input Capture | Ultrasonic rangefinder — road surface profiling |
| **NEO-6M** | UART | GPS receiver — position, altitude & time |
| **TB6612FNG** | GPIO / PWM | Dual motor driver — RC vehicle drive control |

### Communication Interfaces

| Link | Nodes | Protocol |
|---|---|---|
| I²C | STM32 ↔ LSM6DS3 | Register-level IMU configuration & data read |
| UART1 | STM32 ↔ NEO-6M | NMEA 0183 GPS stream (interrupt-driven) |
| UART2 | STM32 ↔ ESP32 | Structured ASCII telemetry packets |
| Wi-Fi | ESP32 ↔ Backend | HTTP POST (JSON telemetry) |
| BLE | Phone ↔ ESP32 | RC vehicle real-time control |

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 5 — WIRING / PINOUT DIAGRAM                           ║
  ║  File: docs/images/wiring.png                                ║
  ║  Suggested size: 1200 × 900 px                               ║
  ║  Use Fritzing or draw.io to draw the wiring between the      ║
  ║  STM32, ESP32, sensors, and motor driver.                    ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Wiring Diagram](docs/images/wiring.png)

---

## 📡 Sensor Subsystems

### 🧲 LSM6DS3 — IMU Shock Detection

The accelerometer continuously monitors vehicle dynamics. A **low-pass gravity estimation** filter runs every sample to cleanly isolate vertical shock from static gravity:

```
Raw Accelerometer Output (x, y, z)
            │
            ▼  exponential low-pass  (α = 0.1)
    Gravity Vector Estimate  [gx, gy, gz]
            │
            ▼  vector subtraction
    Dynamic Acceleration  [ax, ay, az]
            │
            ▼  dot product with gravity unit vector
    Vertical Shock Magnitude  (g-units)
            │
            ▼  threshold comparison
    Road Event Trigger  ──►  Pothole / Rough Road
```

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 6 — IMU SIGNAL WAVEFORM                               ║
  ║  File: docs/images/imu-signal.png                            ║
  ║  Suggested size: 1200 × 500 px                               ║
  ║  Use Arduino Serial Plotter or Matplotlib to plot the raw    ║
  ║  vs filtered IMU signal with a pothole event highlighted.    ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![IMU Signal Plot](docs/images/imu-signal.png)

**Implemented:** I²C init & WHO_AM_I validation · ODR & full-scale configuration · gravity estimation · low-pass filtering · vertical shock extraction · real-time interrupt processing

---

### 📏 HC-SR04 — Ultrasonic Road Surface Measurement

TIM5 hardware input capture measures the echo pulse duration to calculate chassis-to-road distance with microsecond precision:

```
STM32 GPIO Trigger ──► 10 μs pulse
                               │
                               ▼
                        HC-SR04 emits ultrasonic burst
                               │
                               ▼
                         Echo Pin HIGH
                               │  ◄─── TIM5 Capture (rising edge)
                               ▼
                         Echo Pin LOW
                               │  ◄─── TIM5 Capture (falling edge)
                               │
                               ▼
                    Δt  =  t_fall − t_rise
                               │
                               ▼
                    Distance  =  (Δt × 343 m/s) / 2
```

**Implemented:** TIM5 input capture · rising & falling edge detection · hardware-timed pulse width measurement · non-blocking interrupt-driven acquisition

---

### 🛰️ NEO-6M — GPS Telemetry

The GPS module streams **NMEA 0183** sentences continuously. The firmware implements a **ring-buffer producer–consumer** architecture to parse GPS data without ever blocking the main control loop:

```
UART3 RX Interrupt  ──►  Ring Buffer  (producer — ISR context)
                                │
                                ▼
                       Main Loop polls (consumer)
                                │
                                ▼
                     NMEA Sentence Reconstruction
                                │
                                ▼
                          GPGGA Parser
                                │
                                ▼
              ┌─────────────────┼──────────────────┐
          UTC Time          Lat / Lon           Altitude
       Fix Quality      Satellite Count
```

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 7 — GPS MAP TRACK                                     ║
  ║  File: docs/images/gps-track.png                             ║
  ║  Suggested size: 1200 × 700 px                               ║
  ║  Screenshot of the dashboard after a full test run with      ║
  ║  a visible GPS track of the vehicle route plotted on the map ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![GPS Track](docs/images/gps-track.png)

**Implemented:** UART interrupt handling · circular ring buffer · stream-based NMEA reconstruction · GPGGA sentence parsing · UTC time, lat/lon, altitude, fix quality, satellite count

---

## 💾 Firmware Architecture

The repository ships **two STM32 firmware variants** (HAL-based and bare-metal) alongside the ESP32 gateway sketch.

### `firmware-stm32-hal/` — Primary HAL-based Firmware

```
main.c                 ← System init, sensor loop, event classification, packet dispatch
  ├── LSM6DS3.c / .h   ← I²C IMU driver — init, read, gravity filter, shock extraction
  ├── HC_SR04.c / .h   ← TIM5 input-capture ultrasonic driver
  ├── NEO_6M.c  / .h   ← Interrupt-driven GPS UART + ring buffer + GPGGA parser
  └── ESP32.c   / .h   ← UART telemetry framing & transmission to ESP32
```

### `firmware-stm32-baremetal/` — Register-Level Implementation

```
main.c                   ← Bare-metal system init and application loop
  ├── lsm6ds3.c / .h     ← Direct register-level IMU driver (no HAL)
  ├── hc-sr04.c / .h     ← Register-level TIM5 driver
  ├── NEO_6M.c  / .h     ← Bare-metal UART + ring buffer
  └── esp32_comms.c / .h ← Low-level UART telemetry TX
```

> The bare-metal variant is provided as an educational reference demonstrating the same functionality at the register level, without relying on the STM32 HAL abstraction layer.

### `firmware-esp32/road_surface_monitor.ino` — Wi-Fi Gateway

```
road_surface_monitor.ino
  ├── STM32 UART reception & packet parsing
  ├── Dynamic baseline tracking for ultrasonic depth calculation
  ├── Wi-Fi connection management with status logging
  ├── HTTP POST telemetry upload to backend API
  └── BLE UART service for real-time RC vehicle control
```

---

## 📬 Communication Protocol

The STM32 generates structured **ASCII telemetry packets** over UART. Two event types are defined:

| Event | Packet Format |
|---|---|
| **Rough Road** | `R,<shock_g>,<latitude>,<longitude>,<utc_timestamp>` |
| **Pothole** | `P,<shock_g>,<distance_cm>,<latitude>,<longitude>,<utc_timestamp>` |

**Example packets:**
```
R,1.82,12.971598,77.594562,134502.00
P,2.47,18.30,12.971598,77.594562,134512.00
```

The ESP32 parses these, enriches with `device_id`, and HTTP POSTs to the backend:

```http
POST /api/ingest HTTP/1.1
Host: 10.x.x.x:8000
Content-Type: application/json

{
  "device_id":  "esp32-car-01",
  "event":      "pothole",
  "shock":       2.47,
  "distance":   18.30,
  "lat":        12.971598,
  "lon":        77.594562,
  "timestamp":  "134512.00"
}
```

---

## 🌐 Web Dashboard & Backend

<!--
  ╔══════════════════════════════════════════════════════════════╗
  ║  IMAGE 8 — DASHBOARD DETAIL / GIF                            ║
  ║  File: docs/images/dashboard-detail.gif  (or .png)           ║
  ║  Suggested size: 1280 × 720 px                               ║
  ║  Record a short screen capture (GIF via ScreenToGif /        ║
  ║  Kap) showing a new event appearing live on the map.         ║
  ╚══════════════════════════════════════════════════════════════╝
-->
![Dashboard Detail](docs/images/dashboard-detail.gif)

The Python backend (`server.py`) exposes a minimal REST API:

| Endpoint | Method | Description |
|---|---|---|
| `/api/ingest` | `POST` | Receive & store telemetry from ESP32 |
| `/api/events` | `GET` | Retrieve all logged road events |
| `/` | `GET` | Serve the web dashboard |

The frontend (`index.html` + `app.js`) renders all events on an interactive **Leaflet.js** map with color-coded markers and per-event metric cards:

| Marker | Event Type |
|---|---|
| 🔴 **Red** | Pothole — high-severity surface deviation (IMU + ultrasonic confirmed) |
| 🟠 **Orange** | Rough Road — vibration-based surface anomaly (IMU only) |

**Stack:** Python · Leaflet.js · Vanilla JS · CSS Custom Properties · Groq AI integration

---

## 📂 Repository Structure

```
road-condition-monitoring-system/
│
├── firmware-stm32-hal/              ← Primary STM32 firmware (STM32CubeIDE / HAL)
│   ├── Core/
│   │   ├── Src/                     ← main.c, LSM6DS3.c, HC_SR04.c, NEO_6M.c, ESP32.c
│   │   └── Inc/                     ← Header files for all modules
│   └── Drivers/                     ← CMSIS + ST HAL drivers (auto-generated)
│
├── firmware-stm32-baremetal/        ← Register-level reference implementation
│   ├── Core/
│   │   ├── Src/                     ← main.c, lsm6ds3.c, hc-sr04.c, NEO_6M.c, esp32_comms.c
│   │   └── Inc/
│   └── Drivers/
│
├── firmware-esp32/
│   └── road_surface_monitor.ino     ← ESP32 Wi-Fi gateway + BLE RC control
│
├── web-dashboard/
│   ├── server.py                    ← Python backend REST API
│   ├── app.js                       ← Frontend map logic & live event rendering
│   ├── index.html                   ← Dashboard UI shell
│   ├── styles.css                   ← Dark-theme stylesheet
│   └── .env.example                 ← Environment variable template
│
├── model-design/
│   └── Front Leg.stl                ← 3D-printable sensor mount (FDM-ready)
│
└── docs/
    └── images/                      ← ← ← Put all your README images here
        ├── banner.png
        ├── dashboard.png
        ├── dashboard-detail.gif
        ├── architecture.png
        ├── hardware.png
        ├── wiring.png
        ├── imu-signal.png
        └── gps-track.png
```

---

## 🚀 Getting Started

### Prerequisites

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) — for STM32 firmware
- [Arduino IDE](https://www.arduino.cc/en/software) with [ESP32 board support](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- Python 3.9+ — for the backend server
- ST-Link V2/V3 debugger — for flashing the STM32

---

### 1 · Flash the STM32

```bash
# In STM32CubeIDE:
File → Import → General → Existing Projects into Workspace

# Choose one of:
#   firmware-stm32-hal/          ← recommended (HAL-based)
#   firmware-stm32-baremetal/    ← advanced (register-level)

# Build (Ctrl+B) then flash via Run → Debug (ST-Link)
```

---

### 2 · Flash the ESP32

```bash
# Open in Arduino IDE:
firmware-esp32/road_surface_monitor.ino

# Edit the Wi-Fi & server credentials:
const char* WIFI_SSID  = "your_ssid";
const char* WIFI_PASS  = "your_password";
const char* SERVER_URL = "http://<your-server-ip>:8000/api/ingest";

# Board: ESP32S3 Dev Module
# Flash via Arduino IDE → Upload (Ctrl+U)
```

---

### 3 · Run the Backend & Dashboard

```bash
cd web-dashboard

# Copy env template and add your API key
cp .env.example .env
# Edit .env: GROQ_API_KEY=your_key_here

# Install Python dependencies
pip install -r requirements.txt

# Start the server
python server.py

# Dashboard is live at:
# http://localhost:8000
```

---

## 👥 Team

<table>
  <tr>
    <td align="center" width="50%">
      <a href="https://github.com/adarshaudupa">
        <b>Adarsh Udupa</b>
      </a>
      <br><br>
      <b>Firmware Lead & Embedded Integration</b>
      <br><br>
      STM32 system architecture · <code>main.c</code> integration ·
      LSM6DS3 I²C driver · gravity estimation algorithm ·
      vertical shock extraction · HC-SR04 timer input-capture driver ·
      end-to-end subsystem coordination
    </td>
    <td align="center" width="50%">
      <a href="https://github.com/Sumed-Sreedhar">
        <b>Sumed Sreedhar</b>
      </a>
      <br><br>
      <b>GPS Systems & Telemetry Architecture</b>
      <br><br>
      NEO-6M interrupt-driven UART driver · ring buffer architecture ·
      NMEA sentence reconstruction · GPGGA parsing ·
      STM32 ↔ ESP32 packet protocol design · ESP32 firmware ·
      Wi-Fi telemetry pipeline
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <a href="https://github.com/tanmayhutt">
        <b>Tanmay Tiwari</b>
      </a>
      <br><br>
      <b>Backend & IoT Infrastructure</b>
      <br><br>
      Python REST API · database integration · telemetry ingestion pipeline ·
      web dashboard (Leaflet.js) · data visualization ·
      cloud communication architecture · Groq AI integration
    </td>
    <td align="center" width="50%">
      <a href="https://github.com/bandapratham">
        <b>Pratham Banda</b>
      </a>
      <br><br>
      <b>Mechanical Design & Hardware Integration</b>
      <br><br>
      RC vehicle chassis design · AutoCAD modelling ·
      3D-printed sensor mounts · TB6612FNG motor driver integration ·
      power distribution · vehicle wiring & assembly ·
      hardware-level system testing
    </td>
  </tr>
</table>

---

##  Concepts Demonstrated

<details>
<summary><b>Click to expand the full list</b></summary>

<br>

**Embedded Systems**
- Bare-metal register-level peripheral programming
- STM32 HAL abstraction layer usage
- Interrupt-driven firmware architecture
- Timer input capture (TIM5)
- I²C peripheral integration
- UART multi-channel communication
- Non-blocking sensor acquisition

**Signal Processing & Algorithms**
- Low-pass gravity estimation filter
- Dynamic acceleration extraction via vector projection
- Threshold-based event detection
- Ring buffer / circular buffer implementation
- Producer–consumer data flow pattern

**Communication & Networking**
- Inter-MCU UART protocol design
- Structured telemetry packet framing
- NMEA 0183 GPS stream parsing
- Wi-Fi HTTP/REST client (ESP32)
- BLE UART service (ESP32 ↔ mobile)

**Systems Design**
- Distributed embedded processing architecture
- Multi-sensor data fusion fundamentals
- End-to-end embedded-to-cloud data pipeline
- Firmware modularization & driver abstraction
- IoT system integration

</details>

---

##  Future Improvements

| Enhancement | Description |
|---|---|
| 📐 **Coordinate Precision** | GPS decimal-degree conversion & HDOP-based accuracy filtering |
| 💾 **Local Logging** | SD card buffering for offline operation in GPS/Wi-Fi dead zones |
| 🧠 **Adaptive Thresholds** | Self-calibrating event detection that learns road baseline per session |
| 📉 **Kalman Filtering** | Optimal sensor fusion for improved vertical shock estimation |
| 🤖 **ML Classification** | On-device or edge neural network for road condition categorization |
| ☁️ **Cloud Analytics** | Historical data aggregation, heatmaps & trend dashboards |
| 📱 **Mobile Companion App** | Real-time monitoring + RC control in a single mobile app |
| 📶 **V2X Integration** | Vehicle-to-infrastructure telemetry broadcast protocol |
| 🗺️ **Crowdsourced Mapping** | Multi-device deployment feeding a shared city-wide road quality map |

---

##  Current Status

>  **Functional prototype** — All subsystems integrated and field-tested.

| Subsystem | Status |
|---|---|
| LSM6DS3 IMU shock detection |  Working |
| HC-SR04 ultrasonic ranging |  Working |
| NEO-6M GPS acquisition |  Working |
| STM32 ↔ ESP32 UART telemetry |  Working |
| ESP32 Wi-Fi HTTP upload |  Working |
| Python backend REST API |  Working |
| Leaflet.js map dashboard |  Working |
| BLE RC vehicle control |  Working |
| 3D-printed sensor mount |  Designed & printed |
| Bare-metal reference firmware |  Complete |

---

<div align="center">

</div>
