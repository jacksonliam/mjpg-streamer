#!/bin/sh

sudo apt-get -y install cmake
sudo apt-get -y install libjpeg8-dev imagemagick libv4l-dev
make
sudo make install

