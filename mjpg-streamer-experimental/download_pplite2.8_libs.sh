#!/bin/bash

mkdir Paddle-Lite
cd Paddle-Lite/

wget https://paddlelite-demo.bj.bcebos.com/libs/armlinux/paddle_lite_libs_v2_8_0.tar.gz
tar -zxvf paddle_lite_libs_v2_8_0.tar.gz
rm -f paddle_lite_libs_v2_8_0.tar.gz

echo "Done!"
