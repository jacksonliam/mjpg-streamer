#!/bin/sh

#/******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
#******************************************************************************/

## This example shows how to invoke mjpg-streamer from the command line

export LD_LIBRARY_PATH="$(pwd)"
#./mjpg_streamer -i "input_uvc.so --help"

./mjpg_streamer -i "./input_uvc.so" -o "./output_http.so -w ./www"
#./mjpg_streamer -i "./input_uvc.so -d /dev/video0" -i "./input_uvc.so -d /dev/video1" -o "./output_http.so -w ./www"
#valgrind ./mjpg_streamer -i "./input_uvc.so" -o "./output_http.so -w ./www"

#./mjpg_streamer -i "./input_uvc.so" -o "./output_udp.so -p 2001"

## pwd echos the current path you are working at,
## the backticks open a subshell to execute the command pwd first
## the exported variable name configures ldopen() to search a certain
## folder for *.so modules
#export LD_LIBRARY_PATH=`pwd`

## this is the minimum command line to start mjpg-streamer with webpages
## for the input-plugin default parameters are used
#./mjpg_streamer -o "output_http.so -w `pwd`/www"

## to query help for the core:
# ./mjpg_streamer --help

## to query help for the input-plugin "input_uvc.so":
# ./mjpg_streamer --input "input_uvc.so --help"

## to query help for the output-plugin "output_file.so":
# ./mjpg_streamer --output "output_file.so --help"

## to query help for the output-plugin "output_http.so":
# ./mjpg_streamer --output "output_http.so --help"

## to specify a certain device, framerage and resolution for the input plugin:
# ./mjpg_streamer -i "input_uvc.so -d /dev/video2 -r 320x240 -f 10"

## to start both, the http-output-plugin and write to files every 15 second:
# mkdir pics
# ./mjpg_streamer -o "output_http.so -w `pwd`/www" -o "output_file.so -f pics -d 15000"

## to protect the webserver with a username and password (!! can easily get sniffed and decoded, it is just base64 encoded !!)
# ./mjpg-streamer -o "output_http.so -w ./www -c UsErNaMe:SeCrEt"

## If you want to track down errors, use this simple testpicture plugin as input source.
## to use the testpicture input plugin instead of a webcam or folder:
#./mjpg_streamer -i "input_testpicture.so -r 320x240 -d 500" -o "output_http.so -w www"

## The input_file.so plugin watches a folder for new files, it does not matter where
## the JPEG files orginate from. For instance it is possible to grab the desktop and 
## store the files to a folder:
# mkdir -p /tmp/input
# while true; do xwd -root | convert - -scale 640 /tmp/input/bla.jpg; sleep 0.5; done &
## Then the files can be read from the folder "/tmp/input" and served via HTTP
# ./mjpg_streamer -i "input_file.so -f /tmp/input -r" -o "output_http.so -w www"

## To upload files to a FTP server (edit the script first)
# ./mjpg_streamer -i input_testpicture.so -o "output_file.so --command plugins/output_file/examples/ftp_upload.sh"

## To create a control only interface useful for controlling the pan/tilt throug
## a webpage while another program streams video/audio, like skype.
#./mjpg_streamer -i "./input_control.so" -o "./output_http.so -w ./www"

