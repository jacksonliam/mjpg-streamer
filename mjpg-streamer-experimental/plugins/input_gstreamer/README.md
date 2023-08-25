### mjpg-streamer input plugin: input_gstreamer

**MJPEG Streamer with GStreamer input plugin.** This plugin allows you to use any source that GStreamer supports as an input for MJPG Streamer.

### Requirements
- GStreamer development libraries

### Instructions

To use the GStreamer input plugin, set the environment variable `MJPG_GSTREAM_PIPELINE` to your desired GStreamer pipeline. The pipeline should end with `! jpegenc ! appsink`.

Run from the mjpg-streamer folder with:

```
export MJPG_GSTREAM_PIPELINE='your_gstreamer_pipeline_here'
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_gstreamer.so"
```

For example, to use a GStreamer pipeline that captures video from a camera using `nvarguscamerasrc` (e.g. when using a rasberry pi camera on an Nvidia Jetson):

```
export MJPG_GSTREAM_PIPELINE="nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1 ! nvvidconv flip-method=0 ! video/x-raw, width=(int)920, height=(int)540, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! jpegenc ! appsink"
export LD_LIBRARY_PATH=.
./mjpg_streamer -o "output_http.so -w ./www" -i "input_gstreamer.so"
```

### Plugin Options

```
---------------------------------------------------------------
Help for input plugin..: GStreamer input plugin
---------------------------------------------------------------
This plugin accepts no parameters. Instead, set the environment
variable MJPG_GSTREAM_PIPELINE to your desired GStreamer pipeline. 
The pipeline should end with `! jpegenc ! appsink`

Here is the default pipeline, which works when using a rasberry pi camera on an Nvidia Jetson:
"nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1 ! nvvidconv flip-method=0 ! video/x-raw, width=(int)920, height=(int)540, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! jpegenc ! appsink"
---------------------------------------------------------------
```

### Author

Jeremy Vonderfecht  
📧: vonder2 at pdx dot edu
