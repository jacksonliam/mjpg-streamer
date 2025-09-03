#!/bin/bash

# Example QR handler script that demonstrates file-based QR data handling
# This script receives QR data through temporary files instead of environment variables

echo "QR Handler called at $(date)"

if [ -n "$QR_DATA_FILE" ] && [ -n "$QR_DATA_SIZE" ]; then
    # Successful QR decode - data is in a temporary file
    echo "QR Code successfully decoded!"
    echo "Data file: $QR_DATA_FILE"
    echo "Data size: $QR_DATA_SIZE bytes"

    # Read the QR data from the file
    if [ -f "$QR_DATA_FILE" ]; then
        echo "QR Code content:"
        echo "=================="
        cat "$QR_DATA_FILE"
        echo ""
        echo "=================="

        # Example: Process different types of QR codes
        QR_CONTENT=$(cat "$QR_DATA_FILE")

        if [[ "$QR_CONTENT" == http* ]]; then
            echo "Detected URL QR code"
            # Handle URL QR codes
        elif [[ "$QR_CONTENT" == WIFI:* ]]; then
            echo "Detected WiFi QR code"
            # Handle WiFi QR codes
        elif [[ "$QR_CONTENT" =~ ^[0-9]+$ ]]; then
            echo "Detected numeric QR code"
            # Handle numeric QR codes
        else
            echo "Detected text QR code"
            # Handle text QR codes
        fi

        # Clean up the temporary file
        rm -f "$QR_DATA_FILE"
        echo "Temporary file cleaned up"
    else
        echo "ERROR: QR data file $QR_DATA_FILE not found!"
    fi

elif [ -n "$QR_ERROR" ]; then
    # QR decode failed
    echo "QR Code decode failed!"
    echo "Error: $QR_ERROR"

    # You could implement error handling here, such as:
    # - Logging the error
    # - Triggering a different camera exposure
    # - Notifying a monitoring system

else
    echo "ERROR: Unknown QR handler state - no QR_DATA_FILE or QR_ERROR set"
fi

echo "QR Handler finished"
