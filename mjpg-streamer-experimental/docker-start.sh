#!/bin/sh
set -e

export LD_LIBRARY_PATH="/mjpg-streamer/mjpg-streamer-experimental"
./mjpg_streamer -o "$1" -i "$2"