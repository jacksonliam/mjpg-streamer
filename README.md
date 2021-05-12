mjpg-streamer-pplite
====================

这是http://sourceforge.net/projects/mjpg-streamer/ 的分支，并通过input_raspicam插件增加了对Raspberry Pi相机的支持。

此版本嵌入了百度飞桨轻量级深度学习推理框架Paddle Lite，配合OpenCV 3.2，实现了深度学习模型推理视频流的结果通过mjpg-streamer得以在web浏览器上显示。


软硬件测试环境
=======================

本人使用的设备：树莓派3B

操作系统 ubuntu-18.04.4-preinstalled-server-arm64+raspi3 （64位系统）

此OS下载链接: https://pan.baidu.com/s/1unTK_lp1XHlVo1U7jYHCsQ 提取码: chp1

OpenCV版本 3.2.0

CMake版本 3.10.2

Paddle Lite版本2.8


环境配置
=======================

安装如下依赖：

    sudo apt-get install cmake libjpeg8-dev libopencv-dev libprotobuf-c-dev libsdl-dev python3-dev python3-opencv

使用opencv输入还需要安装g++

    sudo apt-get install gcc g++




运行程序
=======================

克隆项目至树莓派上，

然后执行脚本`download_pplite2.8_libs.sh`安装Paddle Lite 2.8的预测库。

```bash
git clone https://github.com/hang245141253/mjpg-streamer-pplite.git
```
```bash
cd mjpg-streamer-pplite/mjpg-streamer-experimental/
```
```bash
sh download_pplite2.8_libs.sh
```


make后执行start.sh（注意提前插入摄像头）
```bash
make && sh start.sh
```

然后通过局域网设备使用浏览器访问你树莓派ip的8080端口，即可查看到一个实时口罩检测的画面。


Discussion / Questions / Help
=============================

Probably best in this thread
http://www.raspberrypi.org/phpBB3/viewtopic.php?f=43&t=45178

Authors
=======

mjpg-streamer was originally created by Tom Stöveken, and has received
improvements from many collaborators since then.


License
=======

mjpg-streamer is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
GNU General Public License for more details.


# FAQ

-- The following OPTIONAL packages have not been found:

 * PythonLibs
 * Numpy
 * SDL

CMake Error: The following variables are used in this project, but they are set to NOTFOUND.


```bash
sudo apt install python3-dev
```
