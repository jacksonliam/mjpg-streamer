mjpg-streamer input plugin: input_uvc
=====================================

This plugin provides JPEG data from V4L/V4L2 compatible webcams.

Usage
=====

    mjpg_streamer -i 'input_uvc.so [options]'
    
```
---------------------------------------------------------------
Help for input plugin..: UVC webcam grabber
---------------------------------------------------------------
The following parameters can be passed to this plugin:

[-d | --device ].......: video device to open (your camera)
[-r | --resolution ]...: the resolution of the video device,
                         can be one of the following strings:
                         QSIF QCIF CGA QVGA CIF VGA 
                         SVGA XGA SXGA 
                         or a custom value like the following
                         example: 640x480
[-f | --fps ]..........: frames per second
                         (activates YUYV format, disables MJPEG)
[-m | --minimum_size ].: drop frames smaller then this limit, useful
                         if the webcam produces small-sized garbage frames
                         may happen under low light conditions
[-e | --every_frame ]..: drop all frames except numbered
[-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver
[-l | --led ]..........: switch the LED "on", "off", let it "blink" or leave
                         it up to the driver using the value "auto"
---------------------------------------------------------------

[-t | --tvnorm ] ......: set TV-Norm pal, ntsc or secam
---------------------------------------------------------------

Optional parameters (may not be supported by all cameras):

[-br ].................: Set image brightness (auto or integer)
[-co ].................: Set image contrast (integer)
[-sh ].................: Set image sharpness (integer)
[-sa ].................: Set image saturation (integer)
[-cb ].................: Set color balance (auto or integer)
[-wb ].................: Set white balance (auto or integer)
[-ex ].................: Set exposure (auto, shutter-priority, aperature-priority, or integer)
[-bk ].................: Set backlight compensation (integer)
[-rot ]................: Set image rotation (0-359)
[-hf ].................: Set horizontal flip (true/false)
[-vf ].................: Set vertical flip (true/false)
[-pl ].................: Set power line filter (disabled, 50hz, 60hz, auto)
[-gain ]...............: Set gain (auto or integer)
[-cagc ]...............: Set chroma gain control (auto or integer)
---------------------------------------------------------------
```
