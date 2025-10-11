mjpg-streamer input plugin: input_opencv
========================================

This input plugin connects to an HTTP mjpg stream, collecting frames and passing 
them on to the main program.

Why would you use this plugin? I have a raspberry pi with a camera in my backyard. 
I use MotionPlus to detect motion and stream the camera. The pi, bless its little heart,
is great but its a zero 2w and with the motion detection and the streaming it gets hot. I 
use mjpg-streamer to have a second pi handle the connection and distribution of the feed 
to multiple destinations. It also allows me to isolate the camera, exposing only the 
stream on the internet.

See https://stackoverflow.com/questions/47729941/mjpeg-over-http-specification for
a discussion of the stream handling.

This version will work with and has been tested with MotionPlus only.

The plugin is designed not to fail, but constantly keep running, even when the 
source is down.

Usage
=====

---------------------------------------------------------------
Help for input plugin..: HTTP Input plugin
---------------------------------------------------------------
                " The following parameters can be passed to this plugin: \
                " [-v | --version ]........: prints the current SVN Revision\n" \
                " [-h | --help]............: show this message\n"
                " [-H | --host]............: select host to data from, localhost is default\n"
                " [-p | --port]............: port, defaults to 8080\n"
                " [-t | --path]............: path to the stream, defaults to '/'"

Use:
    mjpg-streamer -i "input_http.so -H owlcam.local -p 8081 -t /1/mjpg/stream" -o "..."
  
Authors
-------

david@davidhuwmorgan.com (update)
Eugene Katsevman (original)