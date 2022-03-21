#!/bin/bash
RED='\033[0;31m'
NC='\033[0m' # No Color
echo "${RED}Starting install of Raspi Cam${NC}\n" # print that hthe install is starting
cd ~ # Go to the home directory
sudo apt install subversion libjpeg8-dev imagemagick ffmpeg libv4l-dev cmake git -y # Installing the dependacys that are needed to install the software
sudo apt install subversion libjpeg8-dev imagemagick libav-tools libv4l-dev cmake -y
git clone https://github.com/St3v3-B/mjpg-streamer.git # Download the files from github
cd mjpg-streamer/mjpg-streamer-experimental # Go in to the directory that has been downloaded
export LD_LIBRARY_PATH=. # exporting the path to the curent directory
make # making the program