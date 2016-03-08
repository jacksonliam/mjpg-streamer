mjpg-streamer input plugin: input_opencv
========================================

This input plugin uses OpenCV to read from supported video sources, optionally
running the image through a filter plugin that can be specified on the command
line.

If you're not using the image filtering functionality of this plugin, you're
probably better off using some other input plugin as this plugin will probably
consume more CPU resources.

This plugin has only been tested with OpenCV 3.1.0, will probably not work with
OpenCV 2.x without some adjustments.

Usage
=====

```
---------------------------------------------------------------
Help for input plugin..: OpenCV Input plugin
---------------------------------------------------------------
The following parameters can be passed to this plugin:

[-d | --device ].......: video device to open (your camera)
[-r | --resolution ]...: the resolution of the video device,
                         can be one of the following strings:
                         QQVGA QCIF CGA QVGA CIF VGA 
                         SVGA XGA HD SXGA UXGA FHD 
                         
                         or a custom value like the following
                         example: 640x480
[-f | --fps ]..........: frames per second
[-q | --quality ] .....: set quality of JPEG encoding
---------------------------------------------------------------
Optional parameters (may not be supported by all cameras):

[-br ].................: Set image brightness (integer)
[-co ].................: Set image contrast (integer)
[-sh ].................: Set image sharpness (integer)
[-sa ].................: Set image saturation (integer)
[-ex ].................: Set exposure (off, or integer)
[-gain ]...............: Set gain (integer)
---------------------------------------------------------------
Optional filter plugin:
[ -filter ]............: filter plugin .so
[ -fargs ].............: filter plugin arguments
---------------------------------------------------------------
```


Filter plugins
==============

You can specify a filter plugin to load via the "--filter" argument:

    mjpg_streamer -i "input_opencv.so --filter cvfilter_cpp.so" .. 
    
The following plugins are included:

* [cvfilter_cpp](filters/cvfilter_cpp/README.md): barebones example
* [cvfilter_py](filters/cvfilter_py/README.md): Embeds a python interpreter to
  allow you to create a filter script in Python
  
Authors
-------

Dustin Spicuzza (dustin@virtualroadside.com)
