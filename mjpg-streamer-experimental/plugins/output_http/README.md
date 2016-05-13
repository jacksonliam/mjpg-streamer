mjpg-streamer output plugin: output_http
========================================

This plugin streams JPEG data from input plugins via HTTP.

Usage
=====

    mjpg_streamer [input plugin options] -o 'output_http.so [options]'

```
---------------------------------------------------------------
The following parameters can be passed to this plugin:

[-w | --www ]...........: folder that contains webpages in 
                          flat hierarchy (no subfolders)
[-p | --port ]..........: TCP port for this HTTP server
[-c | --credentials ]...: ask for "username:password" on connect
[-n | --nocommands ]....: disable execution of commands
---------------------------------------------------------------
```

Browser/VLC
-----------

To view the stream use VLC or Firefox/Chrome and open the URL:

    http://127.0.0.1:8080/?action=stream

If there are multiple input plugins, you can access each stream individually:

    http://127.0.0.1:8080/?action=stream_0
    http://127.0.0.1:8080/?action=stream_1

To do the same as the GET request above using NSURLSession in Objective-C, a POST request seems to work: 

    POST http://127.0.0.1:8080/stream 

To view a single JPEG just open this URL:

    http://127.0.0.1:8080/?action=snapshot

mplayer
-------

To play the HTTP M-JPEG stream with mplayer:

    # mplayer -fps 30 -demuxer lavf "http://127.0.0.1:8080/?action=stream&ignored.mjpg"

It might be necessary to configure mplayer to prefer IPv4 instead of IPv6:

    # vi ~./mplayer/config
    add or change the option: prefer-ipv4=yes


Notes
=====

If you would like to replace a WebcamXP based system with an mjpg-streamer based
you may use the  WXP_COMPAT argument to cmake. If you compile with this argument
the mjpg stream will be available as cam_1.mjpg and the still jpg snapshot as
cam_1.jpg. 

    # mkdir _build
    # cd _build && cmake -DWXP_COMPAT=ON ..
    # make
