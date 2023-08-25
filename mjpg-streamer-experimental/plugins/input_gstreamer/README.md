mjpg-streamer input plugin: input_gstreamer
==========================================

This plugin reads jpeg encoded images from a GStreamer pipeline. With this plugin, you can use any video source that GStreamer supports as an input for MJPG Streamer.

Requirements
============
- GStreamer development libraries

For example, on Ubuntu:

```
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

Instructions
============

To use the GStreamer input plugin, set the environment variable `MJPG_GSTREAM_PIPELINE` to your desired GStreamer pipeline. The pipeline should end with `! jpegenc ! appsink`.

Run from the mjpg-streamer folder with:

```
export MJPG_GSTREAM_PIPELINE='your_gstreamer_pipeline_here'
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_gstreamer.so"
```

If `MJPG_GSTREAM_PIPELINE` is not set, the default GStreamer pipeline captures video from a camera using `nvarguscamerasrc` (e.g. when using a rasberry pi camera on an Nvidia Jetson):

```
export MJPG_GSTREAM_PIPELINE="nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1 ! nvvidconv flip-method=0 ! video/x-raw, width=(int)920, height=(int)540, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! jpegenc ! appsink"
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_gstreamer.so"
```

Author
------
Jeremy Vonderfecht  
📧: vonder2 at pdx dot edu
