# Road Surface Monitoring Device

## Overview

Road Surface Monitoring Device is a vehicle-mounted embedded sensing and telemetry system designed to detect road anomalies and transmit their locations to a backend server for visualization and analysis.

The system combines:

- STM32F446RE for sensor acquisition and processing
- LSM6DS3 IMU for shock detection
- HC-SR04 ultrasonic sensor for road surface measurement
- NEO-6M GPS module for location tracking
- ESP32-S3 for wireless telemetry transmission
- Backend infrastructure for data storage and visualization

The project combines embedded sensing, GPS telemetry, inter-MCU communication, wireless networking, backend infrastructure, and mechanical vehicle integration to create a complete end-to-end road anomaly monitoring platform.

Road events are detected locally on the STM32, enriched with GPS location data, transmitted to an ESP32 gateway, and uploaded to a backend server over Wi-Fi.

---

# Team

## [Adarsh Udupa](https://github.com/adarshaudupa)
### Firmware Lead & Embedded Integration

Responsible for:

- Overall STM32 firmware integration
- Main application firmware (`main.c`)
- System architecture implementation
- LSM6DS3 driver development
- IMU signal processing implementation
- Gravity estimation algorithm
- Vertical shock extraction logic
- HC-SR04 driver development
- Timer input capture implementation
- Sensor integration and validation
- End-to-end STM32 subsystem coordination

---

## [Sumed Sreedhar](https://github.com/Sumed-Sreedhar)
### GPS Systems & Telemetry Architecture

Responsible for:

- NEO-6M GPS driver development
- Interrupt-driven UART GPS reception
- Ring buffer implementation
- NMEA sentence reconstruction
- GPGGA parsing
- GPS telemetry generation
- STM32 ↔ ESP32 communication design
- ESP32 communication driver development (`ESP32.c`)
- ESP32 firmware development (`road_surface_monitor.ino`)
- Telemetry packet protocol design
- Wi-Fi telemetry transmission integration
- Communication pipeline testing and debugging

---

## [Tanmay Tiwari](https://github.com/tanmayhutt)
### Backend & IoT Infrastructure

Responsible for:

- Backend server development
- API development
- Database integration
- Telemetry ingestion pipeline
- IoT infrastructure
- Web dashboard development
- Cloud communication architecture
- Data visualization and analytics

---

## [Pratham Banda](https://github.com/bandapratham)
### Mechanical Design & Hardware Integration

Responsible for:

- RC vehicle design
- AutoCAD chassis modelling
- Sensor mounting design
- Motor driver integration
- Vehicle wiring and assembly
- Power distribution setup
- Hardware packaging
- Vehicle-level testing and integration

---

# System Architecture

```text
LSM6DS3 IMU
      │
      ▼
Vertical Shock Detection
      │
      ▼
Road Event Classification
      │
      ├────► Rough Road
      │
      └────► Pothole
                    │
                    ▼
             GPS Location
                    │
                    ▼
            STM32 Telemetry
                    │
                    ▼
         STM32 ↔ ESP32 UART
                    │
                    ▼
                 ESP32
                    │
                    ▼
              HTTP Upload
                    │
                    ▼
             Backend Server
                    │
                    ▼
            Web Dashboard
```

---

# Hardware Platform

## Processing Nodes

- STM32F446RE
- ESP32-S3

## Sensors

- LSM6DS3 6-Axis IMU
- HC-SR04 Ultrasonic Sensor
- NEO-6M GPS Receiver

## Vehicle Platform

- RC Car Chassis
- Motor Driver Module

## Communication Interfaces

- UART (STM32 ↔ ESP32)
- UART (STM32 ↔ NEO-6M)
- I2C (STM32 ↔ LSM6DS3)
- Wi-Fi (ESP32 ↔ Backend)

---

# Sensor Subsystems

## LSM6DS3 Shock Detection

The LSM6DS3 accelerometer continuously measures vehicle motion.

A low-pass gravity estimation algorithm separates:

```text
Measured Acceleration
        │
        ▼
Gravity Estimate
        │
        ▼
Dynamic Acceleration
        │
        ▼
Vertical Shock Value
```

The firmware projects dynamic acceleration onto the gravity vector to calculate vertical shock magnitude.

### Implemented Features

- I2C communication
- Accelerometer configuration
- WHO_AM_I validation
- Gravity estimation
- Low-pass filtering
- Vertical shock extraction
- Real-time processing

---

## HC-SR04 Surface Measurement

The HC-SR04 measures distance between the vehicle and road surface.

The driver implements:

- Timer input capture
- Rising edge detection
- Falling edge detection
- Pulse width calculation

Distance is calculated directly from echo pulse duration.

### Implemented Features

- TIM5 input capture
- Hardware timing measurement
- Non-blocking acquisition
- Interrupt-driven distance calculation

---

## NEO-6M GPS Telemetry

The NEO-6M continuously streams NMEA data.

The firmware implements:

- Interrupt-driven UART reception
- Ring buffer architecture
- Producer-consumer data flow
- NMEA sentence reconstruction
- GPGGA parsing

Extracted information includes:

- UTC Time
- Latitude
- Longitude
- Altitude
- Satellite Count
- GPS Fix Quality

### Implemented Features

- UART interrupt handling
- Circular buffering
- Stream processing
- GPS protocol parsing
- Real-time telemetry generation

---

# Inter-MCU Communication

The STM32 and ESP32 communicate through a dedicated UART telemetry link.

The STM32 performs:

- Sensor acquisition
- Event detection
- GPS processing
- Packet generation

The ESP32 performs:

- Packet reception
- Packet parsing
- Network communication
- Backend upload

### Packet Examples

Rough Road Event:

```text
R,shock,latitude,longitude,timestamp
```

Pothole Event:

```text
P,shock,distance,latitude,longitude,timestamp
```

### Concepts Demonstrated

- Inter-MCU communication
- UART protocol design
- Structured packet transmission
- Embedded telemetry systems
- Distributed processing

---

# Road Event Classification

The STM32 classifies road events using sensor measurements.

## Rough Road Detection

Generated when:

- Significant vertical shock is detected

## Pothole Detection

Generated when:

- Significant vertical shock is detected
- Ultrasonic measurement indicates a significant surface deviation

---

# Telemetry Pipeline

```text
STM32
   │
   ▼
Sensor Processing
   │
   ▼
Event Classification
   │
   ▼
UART Telemetry Packet
   │
   ▼
ESP32 Gateway
   │
   ▼
HTTP POST Request
   │
   ▼
Backend API
   │
   ▼
Database
   │
   ▼
Web Dashboard
```

The ESP32 acts as a communication gateway between the sensing node and cloud infrastructure.

---

# Firmware Architecture

## STM32 Firmware

```text
main.c
│
├── LSM6DS3.c
├── HC_SR04.c
├── NEO_6M.c
└── ESP32.c
```

### Responsibilities

- Sensor acquisition
- IMU processing
- GPS processing
- Ultrasonic measurements
- Event classification
- Packet generation
- UART telemetry transmission

---

## ESP32 Firmware

```text
road_surface_monitor.ino
```

### Responsibilities

- UART packet reception
- Packet parsing
- Wi-Fi connectivity
- HTTP communication
- Backend integration
- Telemetry upload

---

# Concepts Demonstrated

- Embedded systems design
- Multi-sensor integration
- Sensor fusion fundamentals
- Interrupt-driven firmware
- UART communication
- Inter-MCU communication
- Ring buffer architectures
- Producer-consumer systems
- GPS telemetry systems
- I2C peripheral integration
- Timer input capture
- Real-time event detection
- Embedded-to-cloud telemetry
- Distributed processing architecture
- IoT system integration
- Firmware subsystem integration
- Embedded telemetry architecture
- GPS stream processing
- End-to-end embedded data pipelines
- Firmware modularization

---

# Repository Structure

```text
Road_Surface_Monitoring_Device/
│
├── Core/
│   ├── Src/
│   │   ├── main.c
│   │   ├── LSM6DS3.c
│   │   ├── HC_SR04.c
│   │   ├── NEO_6M.c
│   │   └── ESP32.c
│   │
│   └── Inc/
│
├── Drivers/
│
├── ESP32_Firmware/
│   └── road_surface_monitor.ino
│
└── README.md
```

---

# Future Improvements

- GPS coordinate conversion to decimal degrees
- SD card data logging
- Adaptive classification thresholds
- Kalman filtering
- Machine-learning-based road condition classification
- Cloud analytics
- Mobile dashboard application
- V2X telemetry integration

---

# Current Status

Functional prototype demonstrating:

- IMU-based shock detection
- Ultrasonic road surface measurement
- GPS location acquisition
- Interrupt-driven telemetry architecture
- Ring-buffer-based GPS processing
- STM32 ↔ ESP32 inter-MCU communication
- Wi-Fi cloud communication
- Backend event ingestion
- Dashboard visualization

The system is capable of detecting road anomalies, associating events with GPS coordinates, transmitting telemetry through an inter-MCU communication pipeline, and visualizing collected data through a backend dashboard.
