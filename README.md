=====*NEW*======

MJPEG-Streamer相比于RTSP等视频方案，非常做适合性能受限的硬件推流，尝试过基于高通410制作了推流器，效果很不错。但为了进一步降低时延，采用将彩色画面转成黑白画面传输(RGB转单Y通道)，并采用一系列加速方式，用法和之前一致。

- 效果：
1. 体积方面：尝试了桌面截图、摄像头照片，传输大小分别降低76%、50%
2. 时间方面：平均增加0.15~0.2s每帧
效果截图见b站视频

- 改动文件：
1. jpeg_utils.c/.h
2. v4l2uvc.c => 加入了一行调用，将输入的图像转换成了Y通道单色，也可根据需要，如动态内容识别/动态分辨率，来进一步降低传输大小。

- 单测：
见test_jpeglib/文件夹
```
gcc test.c -ljpeg
```

- 编译：
```
cd mjpg-streamer-experimental
make distclean
make
# or
make CMAKE_BUILD_TYPE=Debug #DEBUG版本
```

- 使用:
增加--gray参数，加上则是灰色，不加则是原版

=====*NEW(EN)*======

Compared with video schemes such as RTSP, MJPEG-Streamer is very suitable for hardware streaming with limited performance. I have tried to make a streamer based on Qualcomm 410, and the effect is very good. However, in order to further reduce the delay, the color picture is converted into black and white picture transmission (RGB transfer Y channel), and a series of acceleration methods are adopted, the usage is the same as before.

- Effect:
1. Volume: Tried desktop screenshots and camera photos, and the transmission size was reduced by 76% and 50% respectively
2. Time: an average increase of 0.15~0.2s per frame
For the screenshot of the effect, see the video of bilibili.

- Change files:
1. jpeg_utils.c/.h
2. v4l2uvc.c => Added a line of calls to convert the input image into Y channel monochrome, and further reduce the transmission size according to needs, such as dynamic content recognition/dynamic resolution.

- Single test:
See test_jpeglib/ folder
```
gcc test.c -ljpeg
```
- Compile:
```
cd mjpg-streamer-experimental
make distclean
make
# or
make CMAKE_BUILD_TYPE=Debug #DEBUG version
```
- Usage:
`--gray` argument is what you need while using gray mode.


====QUOTE====

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
* ~output_rtsp~ (not functional)
* ~output_udp~ (not functional)
* output_viewer ([documentation](mjpg-streamer-experimental/plugins/output_viewer/README.md))
* output_zmqserver ([documentation](mjpg-streamer-experimental/plugins/output_zmqserver/README.md))

Building & Installation
=======================

You must have cmake installed. You will also probably want to have a development
version of libjpeg installed. I used libjpeg8-dev. e.g.

    sudo apt-get install cmake libjpeg8-dev

If you do not have gcc (and g++ for the opencv plugin) you may need to install those.

    sudo apt-get install gcc g++

Simple compilation
------------------

This will build and install all plugins that can be compiled.

    cd mjpg-streamer-experimental
    make
    sudo make install
    
By default, everything will be compiled in "release" mode. If you wish to compile
with debugging symbols enabled, you can do this:

    cd mjpg-streamer-experimental
    make distclean
    make CMAKE_BUILD_TYPE=Debug
    sudo make install
    
Advanced compilation (via CMake)
--------------------------------

There are options available to enable/disable plugins, setup options, etc. This
shows the basic steps to enable the experimental HTTP management feature:

    cd mjpg-streamer-experimental
    mkdir _build
    cd _build
    cmake -DENABLE_HTTP_MANAGEMENT=ON ..
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
