#!/bin/bash
echo "Starting install of Raspi Cam" # print that hthe install is starting

cd ~ # Go to the home directory

# starting remove of old service if present
systemctl stop raswebcamd
systemctl disable raswebcamd
systemctl daemon-reload
sudo rm -r mjpg-streamer/
sudo rm /etc/systemd/system/raswebcamd.service
# end removing old service 

sudo apt-get install libjpeg-dev git cmake -y # Installing components needed for some of the other programs
sudo apt install subversion libjpeg8-dev imagemagick ffmpeg libv4l-dev cmake git curl -y # Installing the dependacys that are needed to install the software on a desktop system
sudo apt install subversion libjpeg62-turbo-dev imagemagick ffmpeg libv4l-dev cmake git curl -y # Installing the dependacys that are needed to install the software on a arm bassed system

git clone https://github.com/St3v3-B/mjpg-streamer.git # Download the files from github

cd mjpg-streamer/mjpg-streamer-experimental # Go in to the directory that has been downloaded

export LD_LIBRARY_PATH=. # exporting the path to the curent directory

make # making the programa

echo "Making webcamDaemon executable " # print some information

sudo chmod +x /root/mjpg-streamer/scripts/WebcamDaemon # Making the WebcamDaemon executable

echo "installing the raswebcamd service" # print some information

sudo mv /root/mjpg-streamer/Instalation/raswebcamd.service /etc/systemd/system/raswebcamd.service # Moving the rascamd service to the service folder

sudo systemctl daemon-reload # reloading the daemon to include the raswebcam service

sudo systemctl enable raswebcamd # enableling the raswebcamd service

sudo systemctl start raswebcamd # starting the raswebcamd service
