#!/bin/sh
set -e

export LD_LIBRARY_PATH="/usr/local/src/mjpg-streamer-experimental"
./mjpg_streamer -o "$1" -i "$2"