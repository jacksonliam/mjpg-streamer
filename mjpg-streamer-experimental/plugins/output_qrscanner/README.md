# QR Scanner Output Plugin

This output plugin for mjpg-streamer continuously scans incoming video frames for QR codes and executes an external program with the decoded QR data written to temporary files.

## Features

- Real-time QR code scanning of video frames
- Execute external programs with QR data in separate processes (fire-and-forget)
- **File-based data transfer** using mkstemp() for maximum QR code size support
- Configurable scan intervals and backoff mechanisms
- Support for **large QR codes** (up to 7,089 numeric or 4,296 alphanumeric characters)
- Error handling with external program execution for decode failures
- **quirc** QR decoder (mandatory, optimized for embedded systems)

## Dependencies

The plugin requires the following libraries:

- **quirc** (mandatory) - Lightweight QR decoder (https://github.com/dlbeer/quirc)
- **libjpeg** (`libjpeg-dev`) - JPEG decoding

## Installation

### Installing quirc (Required)

```bash
# Build and install quirc
git clone https://github.com/dlbeer/quirc.git
cd quirc
make
sudo make install
```

### Installing dependencies

```bash
# Ubuntu/Debian
sudo apt-get install libjpeg-dev

# CentOS/RHEL/Fedora
sudo yum install libjpeg-devel
```

Build mjpg-streamer with the plugin enabled:

```bash
cd mjpg-streamer-experimental
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so [options]"
```

### Options

- `-i, --input <number>` - Read frames from specified input plugin (default: 0)
  - Use this when you have multiple input plugins and want to select which one to scan
  - Example: `-i 1` reads from the second input plugin
- `-d, --delay <ms>` - Delay between QR scans in milliseconds (default: 1000)
  - Controls how frequently frames are processed for QR codes
  - Minimum value: 100ms
- `-e, --exec <program>` - External program to execute with QR data
  - The decoded QR string will be passed as the `QR_DATA` environment variable
  - Program runs in a separate process (fire-and-forget)
- `-b, --backoff <frames>` - Backoff duration in frames after successful decode (default: 0)
  - Prevents repeated processing of the same QR code
  - Set to 0 to disable backoff and process every detected QR code
  - Example: `-b 10` skips the next 10 frames after detecting a QR code

### Examples

Basic usage with USB camera (QR codes will only be logged):
```bash
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so"
```

With external program execution and backoff (skip 10 frames):
```bash
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so -e /usr/local/bin/qr_handler.sh -b 10"
```

With custom scan interval and no backoff:
```bash
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so -d 500 -e /path/to/handler"
```

Multiple inputs - QR scanner reads from second input with frame-based backoff:
```bash
./mjpg_streamer \
  -i "input_uvc.so -d /dev/video0" \
  -i "input_uvc.so -d /dev/video1" \
  -o "output_http.so -p 8080 -i 0" \
  -o "output_qrscanner.so -i 1 -d 500 -e /home/user/qr_processor.py -b 5"
```

## External Program Integration

When a QR code is detected, the specified external program is executed in a separate process (fire-and-forget) with the decoded QR data available as the `QR_DATA` environment variable.

### Process Management

- External programs run in separate child processes
- The main scanner continues immediately (non-blocking)
- Child processes are automatically reaped by the system
- **Failures are handled gracefully**: Fork or execution failures log warnings but don't terminate the plugin
- **Multiple executable types supported**: Python scripts, shell scripts, compiled binaries, and interpreted programs

### Executable Support

The plugin supports various types of external programs:

**Shell Scripts** (with shebang):
```bash
#!/bin/bash
echo "QR Data: $QR_DATA"
```

**Python Scripts** (with shebang):
```python
#!/usr/bin/env python3
import os
qr_data = os.environ['QR_DATA']
print(f"QR Data: {qr_data}")
```

**Compiled Binaries**:
```bash
# C/C++ compiled program
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so -e /usr/local/bin/my_qr_handler"
```

**Direct Command Execution**:
```bash
# Execute commands directly
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so -e 'echo \$QR_DATA >> /tmp/qr.log'"
```

### Backoff Mechanism

The backoff feature prevents repeated processing of the same QR code:

- After successful QR code detection, scanning is suppressed for the specified number of frames
- Useful when QR codes remain in view for extended periods
- Set to 0 to disable backoff (process every detected QR code)
- Frame-based backoff is simpler and more predictable than time-based backoff
- Example: `-b 5` means skip processing for the next 5 frames after a successful decode

### Error Handling

- **Fork failures**: Log warning, continue QR scanning
- **Environment variable failures**: Log warning in child process
- **Execution failures**: Try direct execution first, then shell interpretation
- **Plugin stability**: External program failures never terminate the main plugin

### Example Handler Script

```bash
#!/bin/bash
# /usr/local/bin/qr_handler.sh

echo "QR Code detected: $QR_DATA"

# Check if it's a WiFi QR code
if [[ "$QR_DATA" == WIFI:* ]]; then
    echo "WiFi QR code detected, processing..."
    # Extract WiFi parameters and configure network
    # Your custom WiFi configuration logic here
elif [[ "$QR_DATA" == http* ]]; then
    echo "URL QR code detected: $QR_DATA"
    # Handle URL QR codes
else
    echo "Generic QR code: $QR_DATA"
    # Handle other QR code types
fi
```

### Python Handler Example

```python
#!/usr/bin/env python3
# /usr/local/bin/qr_handler.py

import os
import sys

qr_data = os.environ.get('QR_DATA', '')
if not qr_data:
    print("No QR data provided")
    sys.exit(1)

print(f"Processing QR code: {qr_data}")

if qr_data.startswith('WIFI:'):
    # Parse and handle WiFi QR codes
    print("WiFi QR code detected")
    # Your WiFi configuration logic here
elif qr_data.startswith('http'):
    # Handle URL QR codes
    print("URL QR code detected")
else:
    # Handle other QR code types
    print("Generic QR code")
```

### Binary/Compiled Program Example

```bash
# Compile a C program that reads QR_DATA environment variable
gcc -o qr_handler qr_handler.c

# Use the compiled binary
./mjpg_streamer -i "input_uvc.so" -o "output_qrscanner.so -e /path/to/qr_handler"
```

## QR Code Formats

The plugin can process any QR code format. Some common examples:

### WiFi QR Codes
```
WIFI:T:WPA;S:MyNetwork;P:MyPassword;H:false;;
```

### URL QR Codes
```
https://example.com
```

### Text QR Codes
```
Any plain text content
```

## Security Considerations

- The `QR_DATA` environment variable is unset after program execution for security
- Ensure your external program validates and sanitizes the QR data before processing
- Consider running the external program with limited privileges

## Troubleshooting

1. **Plugin not building**: Ensure quirc and libjpeg dependencies are installed
2. **External program not executing**: Check program path and permissions
3. **QR codes not detected**: Verify camera focus and lighting conditions
4. **Program execution failed**: Check external program logs and return codes

## License

This plugin is released under the GNU General Public License v2.0, same as mjpg-streamer.
