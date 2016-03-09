input_opencv filter plugin: cvfilter_cpp
========================================

This is just a demonstration plugin that shows how to create a bare minimum
filter plugin for use with the mjpg-streamer input_opencv plugin.

To create your own filter plugin, just copy filter_cpp.cpp to your own project,
and compile/link it with the same build of OpenCV that mjpg-streamer is linked
to.

CMakeLists.txt is specific to the mjpg-streamer build tree, and won't be useful
outside of it.
