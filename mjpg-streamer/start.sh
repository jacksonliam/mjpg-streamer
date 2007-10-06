#!/bin/sh

export LD_LIBRARY_PATH=`pwd`
./mjpg_streamer -o "output_http.so -w `pwd`/www"
