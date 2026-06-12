const DEFAULT_CENTER = [12.9716, 77.5946];
const DEFAULT_ZOOM = 13;
const REFRESH_INTERVAL_MS = 10000;
const SAMPLE_REFRESH_MS = 15000;
const SAMPLE_COUNT = 60;
const SAMPLE_RADIUS_METERS = 600;
const ROAD_SNAP_ENABLED = true;
const ROAD_SAMPLE_ROUTES = 4;
const ROAD_SAMPLE_TIMEOUT_MS = 4000;
const ROAD_SAMPLE_MAX_POINTS = 300;
const OSRM_BASE_URL = "https://router.project-osrm.org";
const API_BASE_URL = window.__API_BASE_URL__ || "http://10.193.26.79:8000";

const map = L.map("map", { zoomControl: true }).setView(DEFAULT_CENTER, DEFAULT_ZOOM);

L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
  maxZoom: 19,
  attribution: "&copy; OpenStreetMap contributors",
}).addTo(map);

const markerLayer = L.layerGroup().addTo(map);

const filters = {
  event: "all",
  minReports: 1,
};

const eventColors = {
  pothole: "#ff5d5d",
  speed_bump: "#ffc857",
  rough_patch: "#4dd599",
};

const eventLabels = {
  pothole: "Pothole",
  speed_bump: "Speed bump",
  rough_patch: "Rough patch",
};

const summaryEl = document.getElementById("summary");
const listEl = document.getElementById("list");
const feedStatusEl = document.getElementById("feedStatus");
const deviceStatusEl = document.getElementById("deviceStatus");
const refreshBtn = document.getElementById("refreshNow");
const eventFilterEl = document.getElementById("eventFilter");
const minReportsEl = document.getElementById("minReports");
const resetBtn = document.getElementById("resetView");
const locationBtn = document.getElementById("toggleLocation");
const locationStatusEl = document.getElementById("locationStatus");

let baseEvents = [];
let sampleEvents = [];
let sampleGeneratedAt = 0;
let sampleCenter = { lat: DEFAULT_CENTER[0], lng: DEFAULT_CENTER[1] };
let sampleCenterReady = false;
let sampleCenterPromise = null;
let locationWatchId = null;
let locationMarker = null;
let locationAccuracyCircle = null;
let isLocationTracking = false;
let hasCenteredOnUser = false;
let lastCenteredEventId = null;

function setFeedStatus(state, message) {
  if (!feedStatusEl) {
    return;
  }
  feedStatusEl.textContent = message;
  feedStatusEl.classList.remove("live", "pending", "error");
  if (state) {
    feedStatusEl.classList.add(state);
  }
}

function setDeviceStatus(state, message) {
  if (!deviceStatusEl) {
    return;
  }
  deviceStatusEl.textContent = message;
  deviceStatusEl.classList.remove("live", "pending", "error");
  if (state) {
    deviceStatusEl.classList.add(state);
  }
}

function setLocationStatus(state, message) {
  if (!locationStatusEl) {
    return;
  }
  locationStatusEl.textContent = message;
  locationStatusEl.classList.remove("live", "pending", "error");
  if (state) {
    locationStatusEl.classList.add(state);
  }
}

function getEventTime(event) {
  if (!event || !event.timestamp) {
    return 0;
  }
  const parsed = Date.parse(event.timestamp);
  return Number.isNaN(parsed) ? 0 : parsed;
}

function formatCoord(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number.toFixed(5) : "-";
}

function formatTimestamp(value) {
  if (!value) {
    return "-";
  }

  const parsed = Date.parse(value);
  if (Number.isNaN(parsed)) {
    return value;
  }

  return new Date(parsed).toLocaleString();
}

function formatPacketType(packetType) {
  const value = String(packetType || "").trim().toUpperCase();

  const labels = {
    P: "Pothole packet",
    B: "Bump packet",
    R: "Rough road packet",
    T: "Legacy telemetry",
  };

  return labels[value] || (value ? `Packet ${value}` : "Packet unknown");
}

function getEventSource(event) {
  if (event?.source) {
    return event.source;
  }

  if (typeof event?.id === "string" && event.id.startsWith("sample-")) {
    return "sample";
  }

  return "live";
}

function formatSourceLabel(event) {
  const source = getEventSource(event);
  return source === "sample" ? "Mock data" : "Live";
}

function formatIntensity(value) {
  const number = Number(value);
  return Number.isFinite(number) ? `${number.toFixed(2)} g` : "-";
}

function formatSpeed(value) {
  const number = Number(value);
  return Number.isFinite(number) ? `${number.toFixed(1)} km/h` : "-";
}

function formatShockRaw(value) {
  const number = Number(value);
  return Number.isFinite(number) ? `${Math.round(number)} raw` : "-";
}

function formatDeviceId(value) {
  return value || "-";
}

function formatRoadMeasurement(event) {
  const delta = Number(event?.ultrasonic_delta_cm);
  if (!Number.isFinite(delta)) {
    return "Ultrasonic: not recorded";
  }

  const signedDelta = `${delta > 0 ? "+" : ""}${delta.toFixed(1)} cm`;
  const magnitude = Math.abs(delta).toFixed(1);

  if (event?.event_type === "pothole" || delta > 0) {
    return `Depth: ${magnitude} cm (${signedDelta} vs baseline)`;
  }

  if (event?.event_type === "speed_bump" || delta < 0) {
    return `Height: ${magnitude} cm (${signedDelta} vs baseline)`;
  }

  return `Deviation: ${signedDelta} vs baseline`;
}

function formatConfidence(value) {
  const number = Number(value);
  return Number.isFinite(number) ? `${Math.round(number * 100)}%` : "-";
}

// --- NEW: Helper to extract raw info from STM32 String ---
function extractSTMData(rawString) {
  if (!rawString || rawString === "No raw data") return null;
  const parts = rawString.trim().split(',');

  // Format based on STM32 sprintf: "R,%d,%s,%s,%lu\n" -> [Type, Shock, Lat, Lon, Timestamp]
  if (parts.length >= 5) {
    return {
      shock: parts[1], // Shock value is always exactly after the first comma
      uptimeMs: parts[parts.length - 1] // Uptime timestamp is always the very last item
    };
  }
  return null;
}
// ----------------------------------------------------------

async function loadEvents() {
  try {
    const apiResponse = await fetch(`${API_BASE_URL}/api/events`, { cache: "no-store" });
    if (apiResponse.ok) {
      const data = await apiResponse.json();
      return { events: data.events || [], source: "api" };
    }
  } catch (error) {
    // Fall back to sample data if the API is unavailable.
  }

  return { events: [], source: "fallback" };
}

async function loadStatus() {
  try {
    const response = await fetch(`${API_BASE_URL}/api/status`, { cache: "no-store" });
    if (response.ok) {
      return await response.json();
    }
  } catch (error) {
    // Ignore status errors.
  }
  return null;
}

function createMarker(event) {
  const color = eventColors[event.event_type] || "#94a3b8";
  const icon = L.divIcon({
    className: "",
    html: `<div style="width:14px;height:14px;border-radius:999px;background:${color};border:2px solid #0f172a;box-shadow:0 0 0 2px rgba(255,255,255,0.2)"></div>`,
  });

  const marker = L.marker([event.lat, event.lng], { icon });
  const confidence = formatConfidence(event.confidence);
  const roadMeasurement = formatRoadMeasurement(event);

  // Parse the raw data to get the exact STM32 readings
  const rawString = event.raw_data || "No raw data";
  const stmData = extractSTMData(rawString);
  const displayShockRaw = stmData ? `${stmData.shock} raw` : formatShockRaw(event.shock_raw);
  const displayUptime = stmData ? `${(Number(stmData.uptimeMs) / 1000).toFixed(1)}s` : "-";

  const popup = `
    <strong>${eventLabels[event.event_type] || "Event"}</strong><br />
    Source: ${formatSourceLabel(event)}<br />
    Packet: ${formatPacketType(event.packet_type)}<br />
    Device: ${formatDeviceId(event.device_id)}<br />
    Intensity: ${formatIntensity(event.intensity_g)}<br />
    Ultrasonic: ${roadMeasurement}<br />
    Shock raw: ${displayShockRaw}<br />
    STM Uptime: ${displayUptime}<br />
    Speed: ${formatSpeed(event.speed_kmh)}<br />
    Raw: <code style="background:#f1f5f9; padding:2px 4px; border-radius:4px; font-size:11px; word-break:break-all;">${rawString}</code><br />
    Time: ${formatTimestamp(event.timestamp)}
  `;
  marker.bindPopup(popup);
  return marker;
}

function renderList(events) {
  listEl.innerHTML = "";

  if (!events.length) {
    listEl.innerHTML = '<div class="empty">No events yet</div>';
    return;
  }

  const sorted = events
    .slice()
    .sort((a, b) => getEventTime(b) - getEventTime(a));
  const visible = sorted.slice(0, 25);

  visible.forEach((event) => {
    const card = document.createElement("div");
    card.className = "card";
    const roadMeasurement = formatRoadMeasurement(event);
    const sourceLabel = formatSourceLabel(event);

    // Parse the raw data to get the exact STM32 readings
    const rawString = event.raw_data || "No raw data";
    const stmData = extractSTMData(rawString);
    const displayShockRaw = stmData ? `${stmData.shock} raw` : formatShockRaw(event.shock_raw);
    const displayUptime = stmData ? `${(Number(stmData.uptimeMs) / 1000).toFixed(1)}s` : "-";

    card.innerHTML = `
      <div class="card-header">
        <div class="card-title">
          <span class="dot ${event.event_type}"></span>
          <span>${eventLabels[event.event_type] || "Event"}</span>
        </div>
        <div class="card-badges">
          <span class="badge ${getEventSource(event)}">${sourceLabel}</span>
          <span class="badge muted">${formatPacketType(event.packet_type)}</span>
        </div>
      </div>
      <div class="metric-grid">
        <div class="metric">
          <span class="metric-label">Device</span>
          <span class="metric-value">${formatDeviceId(event.device_id)}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Intensity</span>
          <span class="metric-value">${formatIntensity(event.intensity_g)}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Ultrasonic</span>
          <span class="metric-value">${roadMeasurement}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Shock raw</span>
          <span class="metric-value">${displayShockRaw}</span>
        </div>
        <div class="metric">
          <span class="metric-label">STM32 Uptime</span>
          <span class="metric-value">${displayUptime}</span>
        </div>
        <div class="metric">
          <span class="metric-label">Confidence</span>
          <span class="metric-value">${formatConfidence(event.confidence)}</span>
        </div>
        <div class="metric" style="grid-column: span 2;">
          <span class="metric-label">Raw STM32 String</span>
          <span class="metric-value" style="font-family: monospace; font-size: 0.85em; word-break: break-all;">${rawString}</span>
        </div>
      </div>
      <div class="card-footer">Lat/Lng: ${formatCoord(event.lat)}, ${formatCoord(event.lng)} | Reports: ${event.report_count ?? "-"}</div>
    `;
    if (Number.isFinite(Number(event.lat)) && Number.isFinite(Number(event.lng))) {
      card.addEventListener("click", () => {
        map.setView([event.lat, event.lng], 17);
      });
    }
    listEl.appendChild(card);
  });
}

function renderSummary(events) {
  if (!events.length) {
    summaryEl.textContent = "No events yet";
    return;
  }

  const total = events.length;
  const potholes = events.filter((e) => e.event_type === "pothole").length;
  const bumps = events.filter((e) => e.event_type === "speed_bump").length;
  const rough = events.filter((e) => e.event_type === "rough_patch").length;
  const liveCount = events.filter((event) => getEventSource(event) === "live").length;
  const sampleCount = events.filter((event) => getEventSource(event) === "sample").length;
  const latestTime = events.reduce((max, event) => Math.max(max, getEventTime(event)), 0);
  const latestLabel = latestTime
    ? ` | last: ${new Date(latestTime).toLocaleTimeString()}`
    : "";
  summaryEl.textContent = `${total} total | ${potholes} potholes | ${bumps} bumps | ${rough} rough | ${liveCount} live | ${sampleCount} mock${latestLabel}`;
}

function hasValidCoords(event) {
  const lat = Number(event.lat);
  const lng = Number(event.lng);
  return Number.isFinite(lat) && Number.isFinite(lng);
}

function applyFilters(events) {
  return events.filter((event) => {
    if (filters.event !== "all" && event.event_type !== filters.event) {
      return false;
    }
    if (event.report_count < filters.minReports) {
      return false;
    }
    return true;
  });
}

function render(events) {
  markerLayer.clearLayers();

  const filtered = applyFilters(events);
  const validEvents = filtered.filter(hasValidCoords);
  validEvents.forEach((event) => createMarker(event).addTo(markerLayer));

  if (validEvents.length > 0) {
    const sortedEvents = [...validEvents].sort((a, b) => getEventTime(b) - getEventTime(a));
    const newestEvent = sortedEvents[0];

    if (newestEvent && newestEvent.id !== lastCenteredEventId) {
      lastCenteredEventId = newestEvent.id;
      map.setView([newestEvent.lat, newestEvent.lng], 16);
    }
  }

  renderSummary(filtered);
  renderList(filtered);
}

function stopLocationTracking(keepStatus = false) {
  if (locationWatchId !== null) {
    navigator.geolocation.clearWatch(locationWatchId);
    locationWatchId = null;
  }

  if (locationMarker) {
    map.removeLayer(locationMarker);
    locationMarker = null;
  }

  if (locationAccuracyCircle) {
    map.removeLayer(locationAccuracyCircle);
    locationAccuracyCircle = null;
  }

  isLocationTracking = false;
  hasCenteredOnUser = false;
  if (locationBtn) {
    locationBtn.classList.remove("active");
    locationBtn.textContent = "Enable location";
  }
  if (!keepStatus) {
    setLocationStatus("pending", "Location: off");
  }
}

function startLocationTracking() {
  if (!navigator.geolocation) {
    setLocationStatus("error", "Location not supported");
    return;
  }

  isLocationTracking = true;
  hasCenteredOnUser = false;
  if (locationBtn) {
    locationBtn.classList.add("active");
    locationBtn.textContent = "Stop location";
  }
  setLocationStatus("pending", "Locating...");

  locationWatchId = navigator.geolocation.watchPosition(
    (position) => {
      const { latitude, longitude, accuracy } = position.coords;
      const coords = [latitude, longitude];

      if (!locationMarker) {
        const icon = L.divIcon({
          className: "",
          html: '<div class="location-marker"></div>',
          iconSize: [14, 14],
          iconAnchor: [7, 7],
        });
        locationMarker = L.marker(coords, { icon, zIndexOffset: 1000 }).addTo(map);
        locationAccuracyCircle = L.circle(coords, {
          radius: accuracy,
          color: "#60a5fa",
          weight: 1,
          fillColor: "#60a5fa",
          fillOpacity: 0.12,
        }).addTo(map);
      } else {
        locationMarker.setLatLng(coords);
        locationAccuracyCircle.setLatLng(coords);
        locationAccuracyCircle.setRadius(accuracy);
      }

      if (!hasCenteredOnUser) {
        map.setView(coords, 16);
        hasCenteredOnUser = true;
      }

      setLocationStatus("live", `Location: ~${Math.round(accuracy)}m`);
    },
    (error) => {
      let message = "Location unavailable";
      if (error.code === error.PERMISSION_DENIED) {
        message = "Location blocked";
      } else if (error.code === error.TIMEOUT) {
        message = "Location timeout";
      }
      setLocationStatus("error", message);
      stopLocationTracking(true);
    },
    {
      enableHighAccuracy: false,
      maximumAge: 10000,
      timeout: 30000,
    }
  );
}

function setupLocationControls() {
  if (!locationBtn) {
    return;
  }

  locationBtn.addEventListener("click", () => {
    if (isLocationTracking) {
      stopLocationTracking();
    } else {
      startLocationTracking();
    }
  });
}

function updateDeviceStatus(status) {
  if (!status || !status.last_ingest) {
    setDeviceStatus("pending", "Device: idle");
    return;
  }

  const lastSeen = Date.parse(status.last_ingest);
  if (Number.isNaN(lastSeen)) {
    setDeviceStatus("pending", "Device: idle");
    return;
  }

  const ageMs = Date.now() - lastSeen;
  const ageSeconds = Math.max(0, Math.round(ageMs / 1000));
  const message = `Device: last seen ${ageSeconds}s ago`;
  if (ageMs <= REFRESH_INTERVAL_MS * 2) {
    setDeviceStatus("live", message);
  } else {
    setDeviceStatus("pending", message);
  }
}

function resolveSampleCenter() {
  if (sampleCenterReady) {
    return Promise.resolve(sampleCenter);
  }
  if (sampleCenterPromise) {
    return sampleCenterPromise;
  }

  sampleCenterPromise = new Promise((resolve) => {
    if (!navigator.geolocation) {
      sampleCenterReady = true;
      resolve(sampleCenter);
      return;
    }

    navigator.geolocation.getCurrentPosition(
      (position) => {
        sampleCenter = {
          lat: position.coords.latitude,
          lng: position.coords.longitude,
        };
        sampleCenterReady = true;
        resolve(sampleCenter);
      },
      () => {
        sampleCenterReady = true;
        resolve(sampleCenter);
      },
      {
        enableHighAccuracy: false,
        timeout: 5000,
        maximumAge: 60000,
      }
    );
  });

  return sampleCenterPromise;
}

function randomBetween(min, max) {
  return min + Math.random() * (max - min);
}

function randomPointAround(center, radiusMeters) {
  const angle = Math.random() * Math.PI * 2;
  const distance = Math.sqrt(Math.random()) * radiusMeters;
  const dx = Math.cos(angle) * distance;
  const dy = Math.sin(angle) * distance;
  const latOffset = dy / 111320;
  const lngOffset = dx / (111320 * Math.cos((center.lat * Math.PI) / 180));
  return {
    lat: center.lat + latOffset,
    lng: center.lng + lngOffset,
  };
}

async function fetchRoutePoints(start, end) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), ROAD_SAMPLE_TIMEOUT_MS);
  const url = `${OSRM_BASE_URL}/route/v1/driving/${start.lng},${start.lat};${end.lng},${end.lat}?overview=full&geometries=geojson`;

  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) {
      return [];
    }
    const data = await response.json();
    const coords = data.routes?.[0]?.geometry?.coordinates || [];
    return coords.map(([lng, lat]) => ({ lat, lng }));
  } catch (error) {
    return [];
  } finally {
    clearTimeout(timeout);
  }
}

async function getRoadSamplePoints(center) {
  const routePromises = [];

  for (let i = 0; i < ROAD_SAMPLE_ROUTES; i += 1) {
    const start = randomPointAround(center, SAMPLE_RADIUS_METERS);
    const end = randomPointAround(center, SAMPLE_RADIUS_METERS);
    routePromises.push(fetchRoutePoints(start, end));
  }

  const routeResults = await Promise.all(routePromises);
  const points = routeResults.flat();
  if (!points.length) {
    return [];
  }

  if (points.length <= ROAD_SAMPLE_MAX_POINTS) {
    return points;
  }

  const step = Math.ceil(points.length / ROAD_SAMPLE_MAX_POINTS);
  return points.filter((_, index) => index % step === 0);
}

function pickEventType() {
  const roll = Math.random();
  if (roll < 0.45) {
    return "pothole";
  }
  if (roll < 0.75) {
    return "speed_bump";
  }
  return "rough_patch";
}

function intensityFor(type) {
  if (type === "pothole") {
    return Number(randomBetween(2.6, 3.8).toFixed(1));
  }
  if (type === "speed_bump") {
    return Number(randomBetween(1.4, 2.1).toFixed(1));
  }
  return Number(randomBetween(0.9, 1.5).toFixed(1));
}

function ultrasonicDeltaFor(type) {
  if (type === "pothole") {
    return Number(randomBetween(2.0, 7.5).toFixed(1));
  }
  if (type === "speed_bump") {
    return Number(randomBetween(-6.0, -1.5).toFixed(1));
  }
  return Number(randomBetween(-1.5, 1.5).toFixed(1));
}

function generateSampleEvents(center, count, radiusMeters) {
  const events = [];
  const baseTime = Date.now();
  const latRadians = (center.lat * Math.PI) / 180;

  for (let i = 0; i < count; i += 1) {
    const angle = Math.random() * Math.PI * 2;
    const distance = Math.sqrt(Math.random()) * radiusMeters;
    const dx = Math.cos(angle) * distance;
    const dy = Math.sin(angle) * distance;
    const latOffset = dy / 111320;
    const lngOffset = dx / (111320 * Math.cos(latRadians));
    const eventType = pickEventType();
    const packetType = eventType === "pothole" ? "P" : eventType === "speed_bump" ? "B" : "R";
    const intensity = intensityFor(eventType);

    events.push({
      id: `sample-${baseTime}-${i + 1}`,
      timestamp: new Date(baseTime - Math.random() * 2 * 60 * 60 * 1000).toISOString(),
      lat: center.lat + latOffset,
      lng: center.lng + lngOffset,
      event_type: eventType,
      packet_type: packetType,
      device_id: "mock-device-01",
      intensity_g: intensity,
      ultrasonic_delta_cm: ultrasonicDeltaFor(eventType),
      shock_raw: Math.round(intensity * 16384 * randomBetween(0.85, 1.15)),
      speed_kmh: Number(randomBetween(12, 42).toFixed(1)),
      report_count: Math.floor(randomBetween(1, 10)),
      source: "sample",
    });
  }

  return events;
}

function generateSampleEventsOnRoads(roadPoints, count) {
  const events = [];
  const baseTime = Date.now();

  for (let i = 0; i < count; i += 1) {
    const point = roadPoints[Math.floor(Math.random() * roadPoints.length)];
    const eventType = pickEventType();
    const packetType = eventType === "pothole" ? "P" : eventType === "speed_bump" ? "B" : "R";
    const intensity = intensityFor(eventType);

    events.push({
      id: `sample-${baseTime}-${i + 1}`,
      timestamp: new Date(baseTime - Math.random() * 2 * 60 * 60 * 1000).toISOString(),
      lat: point.lat,
      lng: point.lng,
      event_type: eventType,
      packet_type: packetType,
      device_id: "mock-device-01",
      intensity_g: intensity,
      ultrasonic_delta_cm: ultrasonicDeltaFor(eventType),
      shock_raw: Math.round(intensity * 16384 * randomBetween(0.85, 1.15)),
      speed_kmh: Number(randomBetween(12, 42).toFixed(1)),
      report_count: Math.floor(randomBetween(1, 10)),
      source: "sample",
    });
  }

  return events;
}

async function getSampleEvents() {
  const now = Date.now();
  if (sampleEvents.length && now - sampleGeneratedAt < SAMPLE_REFRESH_MS) {
    return sampleEvents;
  }

  const center = await resolveSampleCenter();
  if (ROAD_SNAP_ENABLED) {
    const roadPoints = await getRoadSamplePoints(center);
    if (roadPoints.length) {
      sampleEvents = generateSampleEventsOnRoads(roadPoints, SAMPLE_COUNT);
    } else {
      sampleEvents = generateSampleEvents(center, SAMPLE_COUNT, SAMPLE_RADIUS_METERS);
    }
  } else {
    sampleEvents = generateSampleEvents(center, SAMPLE_COUNT, SAMPLE_RADIUS_METERS);
  }
  sampleGeneratedAt = now;
  return sampleEvents;
}

function setupControls() {
  if (eventFilterEl) {
    eventFilterEl.addEventListener("change", (event) => {
      filters.event = event.target.value;
      render(baseEvents);
    });
  }

  if (minReportsEl) {
    minReportsEl.addEventListener("input", (event) => {
      filters.minReports = Number(event.target.value || 1);
      render(baseEvents);
    });
  }

  if (resetBtn) {
    resetBtn.addEventListener("click", () => {
      map.setView(DEFAULT_CENTER, DEFAULT_ZOOM);
    });
  }

  setupLocationControls();
}

async function refreshEvents() {
  setFeedStatus("pending", "Fetching...");

  try {
    const [eventsResult, status] = await Promise.all([loadEvents(), loadStatus()]);
    let { events, source } = eventsResult;

    if (source !== "api" || events.length === 0) {
      events = await getSampleEvents();
      source = "sample";
    } else {
      events = events.map((event) => ({
        ...event,
        source: "live",
      }));
    }

    baseEvents = events;
    render(baseEvents);

    const timeLabel = new Date().toLocaleTimeString();
    if (source === "api") {
      setFeedStatus("live", `Live · ${timeLabel}`);
      updateDeviceStatus(status);
    } else {
      setFeedStatus("pending", `Sample data · ${timeLabel}`);
      setDeviceStatus("error", "Device: offline");
    }

    return events;
  } catch (error) {
    setFeedStatus("error", "API offline");
    setDeviceStatus("error", "Device: unknown");
    summaryEl.textContent = error.message;
    return [];
  }
}

setupControls();

if (refreshBtn) {
  refreshBtn.addEventListener("click", () => {
    refreshEvents();
  });
}

refreshEvents();

setInterval(() => {
  refreshEvents();
}, REFRESH_INTERVAL_MS);

