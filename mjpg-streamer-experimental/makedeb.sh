#!/bin/bash

rm -rf _pkg
install -Dm755 mjpg_streamer _pkg/usr/bin/mjpg_streamer
install -d _pkg/usr/lib
install *.so _pkg/usr/lib
install -D mjpg_streamer@.service _pkg/lib/systemd/system/mjpg_streamer@.service
mkdir -p _pkg/usr/share/mjpg_streamer
cp -r www _pkg/usr/share/mjpg_streamer/www

version="$(grep '#define SOURCE_VERSION' mjpg_streamer.h | awk '{gsub(/"/, ""); print $3}')"
fpm --output-type deb --input-type dir --chdir _pkg --after-install postinstall.sh --name mjpg-streamer --version $version
