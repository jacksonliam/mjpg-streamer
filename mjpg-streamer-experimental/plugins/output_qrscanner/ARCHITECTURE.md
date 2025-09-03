# QR Scanner Plugin Architecture

## Overview

The mjpg-streamer QR Scanner output plugin is designed with the following architecture:

## Core Components

### 1. Plugin Interface (`output_qrscanner.h/.c`)
- Implements the standard mjpg-streamer output plugin interface
- Functions: `output_init()`, `output_run()`, `output_stop()`, `output_cmd()`
- Manages plugin lifecycle and parameter parsing
- **Requires quirc library** - no conditional compilation

### 2. Worker Thread
- Continuously monitors input frames from the specified input plugin
- Processes frames at configurable intervals (default: 1000ms)
- Handles frame copying and synchronization with input plugin

### 3. QR Code Processing Pipeline
- **Frame Capture**: Gets JPEG frames from input plugin buffer
- **Image Decoding**: Uses libjpeg to decode JPEG to grayscale format
- **QR Detection**: Uses quirc library to scan for QR codes
- **WiFi Parsing**: Parses WiFi QR code format (`WIFI:T:WPA;S:ssid;P:pass;H:false;;`)
- **Network Configuration**: Sends configuration to ConnMan via DBus

### 4. ConnMan Integration
- Uses DBus system bus to communicate with ConnMan daemon
- Sends WiFi service configuration messages
- Supports multiple security types (WPA, WPA2, WEP, Open)

## Data Flow

```
Input Plugin → Frame Buffer → Worker Thread → libjpeg Decoder → quirc Scanner → WiFi Parser → DBus → ConnMan
```

## Thread Safety
- Uses pthread mutex/condition variables for frame synchronization
- Maintains separate frame buffer in worker thread
- Proper cleanup on thread termination

## Error Handling
- Graceful degradation when dependencies are missing
- DBus connection error handling
- Image processing error recovery
- Memory allocation failure handling

## Configuration
- Input plugin selection (`-i`)
- Scan interval adjustment (`-d`)
- Runtime parameter modification via plugin command interface

## Security Considerations
- WiFi passwords handled in memory only
- DBus system bus permissions required
- Network configuration requires appropriate privileges
