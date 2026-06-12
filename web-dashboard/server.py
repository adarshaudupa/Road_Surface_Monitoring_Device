import json
import os
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, Tuple

from dotenv import load_dotenv  # <-- Added to load .env files
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from openai import OpenAI
from pydantic import BaseModel, Field

# Load environment variables from a .env file if it exists
load_dotenv()

BASE_DIR = Path(__file__).resolve().parent
DATA_FILE = BASE_DIR / "data" / "potholes.json"

# --- CONFIGURATION (Loaded from environment) ---
GROQ_API_KEY = os.environ.get("GROQ_API_KEY")
# Falls back to llama3-8b-8192 if not specified in the .env file
GROQ_MODEL = os.environ.get("GROQ_MODEL", "llama3-8b-8192")
# -----------------------------------------------

client = None
if GROQ_API_KEY:
    client = OpenAI(
        api_key=GROQ_API_KEY,
        base_url="https://api.groq.com/openai/v1",
    )

class IngestPayload(BaseModel):
    timestamp: Optional[str] = None
    device_id: Optional[str] = None
    event_type: Optional[str] = None
    packet_type: Optional[str] = None
    lat: float
    lng: float
    accel_peak_g: float = Field(..., ge=0)
    shock_raw: Optional[int] = None
    shock_duration_ms: Optional[int] = None
    ultrasonic_delta_cm: Optional[float] = None
    speed_kmh: Optional[float] = None
    raw_data: Optional[str] = None

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

def load_events():
    if not DATA_FILE.exists():
        return []
    try:
        with DATA_FILE.open("r", encoding="utf-8") as file:
            data = json.load(file)
        return data.get("events", [])
    except (json.JSONDecodeError, OSError):
        return []

def save_events(events):
    DATA_FILE.parent.mkdir(parents=True, exist_ok=True)
    with DATA_FILE.open("w", encoding="utf-8") as file:
        json.dump({"events": events}, file, indent=2)

def normalize_event_type(event_type: Optional[str]) -> Optional[str]:
    if not event_type:
        return None

    normalized = event_type.strip().lower()
    aliases = {
        "p": "pothole",
        "pothole": "pothole",
        "b": "speed_bump",
        "speed_bump": "speed_bump",
        "speed bump": "speed_bump",
        "r": "rough_patch",
        "rough_patch": "rough_patch",
        "rough patch": "rough_patch",
    }
    if normalized in aliases:
        return aliases[normalized]
    return None

def rule_based_classify(payload: IngestPayload) -> Tuple[str, float]:
    ultrasonic_delta = payload.ultrasonic_delta_cm or 0.0

    if payload.accel_peak_g >= 2.6 and ultrasonic_delta > 3.0:
        return "pothole", 0.6
    if payload.accel_peak_g >= 1.5 and ultrasonic_delta < -2.0:
        return "speed_bump", 0.55
    return "rough_patch", 0.5

def call_groq(payload: IngestPayload) -> Optional[Tuple[str, float]]:
    if client is None:
        return None

    features = {
        "accel_peak_g": payload.accel_peak_g,
        "shock_duration_ms": payload.shock_duration_ms,
        "ultrasonic_delta_cm": payload.ultrasonic_delta_cm,
        "speed_kmh": payload.speed_kmh,
    }

    prompt = (
        "You are a road anomaly classifier. "
        "Return ONLY valid JSON with keys 'event_type' and 'confidence'. "
        "event_type must be exactly one of: pothole, speed_bump, rough_patch. "
        "confidence must be a number between 0 and 1. "
        f"Features: {features}"
    )

    try:
        response = client.chat.completions.create(
            model=GROQ_MODEL,
            messages=[{"role": "user", "content": prompt}],
            response_format={"type": "json_object"}
        )

        output = response.choices[0].message.content.strip()
        result = json.loads(output)

        event_type = result.get("event_type")
        confidence = float(result.get("confidence", 0))

        if event_type not in {"pothole", "speed_bump", "rough_patch"}:
            raise ValueError("Invalid event_type")

        confidence = max(0.0, min(1.0, confidence))
        return event_type, confidence

    except Exception as e:
        print(f"API Error: {e}")
        return None

def classify_event(payload: IngestPayload) -> Tuple[str, float]:
    device_event_type = normalize_event_type(payload.event_type)
    if device_event_type is None:
        device_event_type = normalize_event_type(payload.packet_type)
    if device_event_type is not None:
        return device_event_type, 1.0

    try:
        groq_result = call_groq(payload)
        if groq_result is not None:
            return groq_result
    except (ValueError, json.JSONDecodeError, OSError):
        pass

    return rule_based_classify(payload)

def build_event(payload: IngestPayload, event_type: str, confidence: float):
    timestamp = payload.timestamp
    if not timestamp:
        timestamp = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

    event_id = f"evt-{int(time.time() * 1000)}"

    return {
        "id": event_id,
        "timestamp": timestamp,
        "device_id": payload.device_id,
        "packet_type": payload.packet_type,
        "shock_raw": payload.shock_raw,
        "lat": payload.lat,
        "lng": payload.lng,
        "event_type": event_type,
        "intensity_g": payload.accel_peak_g,
        "ultrasonic_delta_cm": payload.ultrasonic_delta_cm,
        "speed_kmh": payload.speed_kmh,
        "report_count": 1,
        "confidence": confidence,
        "source": "live",
        "raw_data": payload.raw_data
    }

def convert_nmea_to_dd(val: float) -> float:
    if val == 0.0:
        return 0.0
    if abs(val) > 180.0:
        degrees = int(val / 100)
        minutes = val - (degrees * 100)
        return degrees + (minutes / 60.0)
    return val

events_cache = load_events()
last_ingest_time: Optional[datetime] = None
last_ingest_event_id: Optional[str] = None
last_ingest_device_id: Optional[str] = None

@app.get("/api/health")
def health_check():
    return {
        "status": "ok",
        "groq_model": GROQ_MODEL,
        "groq_enabled": client is not None,
    }

@app.get("/api/events")
def get_events():
    return {"events": events_cache}

@app.get("/api/status")
def get_status():
    timestamp = None
    if last_ingest_time:
        timestamp = last_ingest_time.isoformat().replace("+00:00", "Z")

    return {
        "last_ingest": timestamp,
        "last_event_id": last_ingest_event_id,
        "last_device_id": last_ingest_device_id,
        "event_count": len(events_cache),
    }

@app.post("/api/ingest")
def ingest(payload: IngestPayload):
    payload.lat = convert_nmea_to_dd(payload.lat)
    payload.lng = convert_nmea_to_dd(payload.lng)

    if payload.lat == 0 and payload.lng == 0:
        raise HTTPException(status_code=400, detail="Invalid coordinates")

    event_type, confidence = classify_event(payload)
    event = build_event(payload, event_type, confidence)

    events_cache.append(event)
    save_events(events_cache)

    global last_ingest_time
    global last_ingest_event_id
    global last_ingest_device_id
    last_ingest_time = datetime.now(timezone.utc)
    last_ingest_event_id = event.get("id")
    last_ingest_device_id = payload.device_id

    return event

app.mount("/", StaticFiles(directory=BASE_DIR, html=True), name="static")
