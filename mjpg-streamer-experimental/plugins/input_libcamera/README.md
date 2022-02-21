mjpg-streamer input plugin: input_libcamera
============================================

Usage
=====

```
---------------------------------------------------------------
Help for input plugin..: Libcamera Input plugin
---------------------------------------------------------------
The following parameters can be passed to this plugin:

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
[-sa ].................: Set image saturation (integer)
[-ex ].................: Set exposure (off, or integer)
[-gain ]...............: Set gain (integer)
---------------------------------------------------------------
```
