mjpg-streamer input plugin: input_raspicam
==========================================

MJPEG Streamer with raspicam input plugin (based on raspistill mmal source code)   

Discussion / Questions / Help
=============================

Probably best in this thread
http://www.raspberrypi.org/phpBB3/viewtopic.php?f=43&t=45178


Instructions
============

If you ran the basic build, you can run from the mjpeg streamer experimental
folder with:
```
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam.so"
```

You can specify options, like in raspivid:
```
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam.so -x 1280 -y 720 -fps 15 -ex night"
```

It does support upto 1080p 30fps, but the bandwidth produced would be more than the usb bus (and therefore ethernet port / wifi dongle) can provide. 720p 15fps is a good compromise.

Here's some help for this input plugin:
```
 ---------------------------------------------------------------
 Help for input plugin..: raspicam input plugin
 ---------------------------------------------------------------
 The following parameters can be passed to this plugin:

 [-fps | --framerate]...: set video framerate, default 5 frame/sec
 [-x | --width ]........: width of frame capture, default 640
 [-y | --height]........: height of frame capture, default 480
 [-quality].............: set JPEG quality 0-100, default 85
 [-usestills]...........: uses stills mode instead of video mode
 [-preview].............: enable full screen preview
 
 -sh : Set image sharpness (-100 to 100)
 -co : Set image contrast (-100 to 100)
 -br : Set image brightness (0 to 100)
 -sa : Set image saturation (-100 to 100)
 -ISO : Set capture ISO
 -vs : Turn on video stabilisation
 -ev : Set EV compensation
 -ex : Set exposure mode (see raspistill notes)
 -awb : Set AWB mode (see raspistill notes)
 -ifx : Set image effect (see raspistill notes)
 -cfx : Set colour effect (U:V)
 -mm : Set metering mode (see raspistill notes)
 -rot : Set image rotation (0-359)
-stats : Compute image stats for each picture (reduces noise)
 -drc : Dynamic range compensation level (see raspistill notes)
 -hf : Set horizontal flip
 -vf : Set vertical flip
 ---------------------------------------------------------------

```
Some of the camera options like ISO may not work due to it not working in the mmal-libs.

Video mode is the default as it allows much smoother video (higher framerates).
Stills mode allows you to use the full-frame of the sensor, but has a max framerate of around 8fps, probably less.
Use stills mode with low FPS (e.g. 1 or 2).

In order to have preview output shown on the raspi screen add the -preview option.

This should run indefinitely. ctrl-c closes mjpeg streamer and raspicam gracefully.

Based on https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiStill.c
modified mmal header and source files from https://github.com/raspberrypi/userland/tree/master/interface/mmal
