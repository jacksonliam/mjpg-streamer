mjpg-streamer
=============

This is a fork of http://sourceforge.net/projects/mjpg-streamer/ with added support for the Raspberry Pi camera via the input_raspicam plugin.

mjpg-streamer is a command line application that copies JPEG frames from one
or more input plugins to multiple output plugins. It can be used to stream
JPEG files over an IP-based network from a webcam to various types of viewers
such as Chrome, Firefox, Cambozola, VLC, mplayer, and other software capable
of receiving MJPG streams.

It was originally written for embedded devices with very limited resources in
terms of RAM and CPU. Its predecessor "uvc_streamer" was created because
Linux-UVC compatible cameras directly produce JPEG-data, allowing fast and
perfomant M-JPEG streams even from an embedded device running OpenWRT. The
input module "input_uvc.so" captures such JPG frames from a connected webcam.
mjpg-streamer now supports a variety of different input devices.

Security warning
----------------

**WARNING**: mjpg-streamer should not be used on untrusted networks!
By default, anyone with access to the network that mjpg-streamer is running
on will be able to access it.

Plugins
-------

Input plugins:

* input_file
* input_http
* input_opencv ([documentation](mjpg-streamer-experimental/plugins/input_opencv/README.md))
* input_ptp2
* input_raspicam ([documentation](mjpg-streamer-experimental/plugins/input_raspicam/README.md))
* input_uvc ([documentation](mjpg-streamer-experimental/plugins/input_uvc/README.md))

Output plugins:

* output_file
* output_http ([documentation](mjpg-streamer-experimental/plugins/output_http/README.md))
* output_rtsp
* output_udp
* output_viewer ([documentation](mjpg-streamer-experimental/plugins/output_viewer/README.md))

Building & Installation
=======================

You must have cmake installed. You will also probably want to have a development
version of libjpeg installed. I used libjpeg8-dev. e.g.

    sudo apt-get install cmake libjpeg8-dev

Simple compilation
------------------

This will build and install all plugins that can be compiled.

    cd mjpg-streamer-experimental
    make
    sudo make install
    
By default, everything will be compiled in "release" mode. If you wish to compile
with debugging symbols enabled, you can do this:

    cd mjpg-streamer-experimental
    make CMAKE_BUILD_TYPE=Debug
    sudo make install
    
Advanced compilation (via CMake)
--------------------------------

There are options available to enable/disable plugins, setup options, etc. This
shows the basic steps.

    cd mjpg-streamer-experimental
    mkdir _build
    cd _build
    cmake ..
    make
    sudo make install

Usage
=====
From the mjpeg streamer experimental
folder:
```
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam.so"
```

See [README.md](mjpg-streamer-experimental/README.md) or the individual plugin's documentation for more details.

Discussion / Questions / Help
=============================

Probably best in this thread
http://www.raspberrypi.org/phpBB3/viewtopic.php?f=43&t=45178

Authors
=======

mjpg-streamer was originally created by Tom Stöveken, and has received
improvements from many collaborators since then.


License
=======

mjpg-streamer is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
GNU General Public License for more details.
