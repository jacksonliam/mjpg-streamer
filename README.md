mjpg-streamer
=============
MJPEG Streamer with raspicam input plugin (based on raspistill mmal source code)

Simply compile with 'make clean all' from within the mjpeg streamer experimental folder

Youll need to have cmake and a dev version of libjpeg installed. I used libjpeg62-dev.

you can run from that dir with:
```
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam.so"
```

Here's some Help:
```
 ---------------------------------------------------------------
 Help for input plugin..: raspicam input plugin
 ---------------------------------------------------------------
 The following parameters can be passed to this plugin:

 [-fps | --framerate]...: set video framerate, default 1 frame/sec
 [-x | --width ]........: width of frame capture, default 640
 [-y | --height]....: height of frame capture, default 480
 [-y | --height]....: height of frame capture, default 480
 [-quality]....: set JPEG quality 0-100, default 85
 [-usestills]....: uses stills mode instead of video mode

 -sh : Set image sharpness (-100 to 100)
 -co : Set image contrast (-100 to 100)
 -br : Set image brightness (0 to 100)
 -sa : Set image saturation (-100 to 100)
 -ISO : Set capture ISO
 -vs : Turn on video stablisation
 -ev : Set EV compensation
 -ex : Set exposure mode (see raspistill notes)
 -awb : Set AWB mode (see raspistill notes)
 -ifx : Set image effect (see raspistill notes)
 -cfx : Set colour effect (U:V)
 -mm : Set metering mode (see raspistill notes)
 -rot : Set image rotation (0-359)
 -hf : Set horizontal flip
 -vf : Set vertical flip
 ---------------------------------------------------------------

```
ISO doesn't work due to it not working in raspistill.
Minimum working delay seems to be about 100ms to give you about 8fps.
There's no preview output shown on the raspi screen.

This should run indefinately. 
ctrl-c closes mjpeg streamer and raspicam gracefully.


Fork of http://sourceforge.net/projects/mjpg-streamer/
and based on https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiStill.c
modified mmal header and source files from https://github.com/raspberrypi/userland/tree/master/interface/mmal
