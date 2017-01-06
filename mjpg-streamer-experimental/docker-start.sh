#!/bin/sh
set -e

if [ "$1" = 'postgres' ]; then
    chown -R postgres "$PGDATA"

    if [ -z "$(ls -A "$PGDATA")" ]; then
        gosu postgres initdb
    fi

    exec gosu postgres "$@"
fi

export LD_LIBRARY_PATH="$(pwd)"
./mjpg_streamer -o "$1" -i "$2"