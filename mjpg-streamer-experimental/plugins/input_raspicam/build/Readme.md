Cmake runs in this directory to keep the parent dir clean. Its called from the mjpeg-streamer makefile, which then calls make in this directory and finally copies the built library file to the correct place.
mjpeg-streamer's "make clean" deletes the contents of this directory, including this readme.
