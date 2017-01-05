FROM ubuntu:16.04

# Install all necessary packages for OpenCV
RUN apt-get update && apt-get upgrade -y \
    && apt-get install -y wget unzip git build-essential cmake pkg-config \
            libjpeg8-dev libtiff5-dev libjasper-dev libpng12-dev \
            libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
            libxvidcore-dev libx264-dev \
            libgtk-3-dev \
            libatlas-base-dev gfortran \
            python3.5-dev

WORKDIR /code

# Configure python
RUN wget -q https://bootstrap.pypa.io/get-pip.py \
    && python get-pip.py
RUN pip install numpy

# Download and build OpenCV source code
RUN wget -q -O opencv.zip https://github.com/opencv/opencv/archive/3.2.0.zip \  
    && wget -q -O opencv_contrib.zip https://github.com/opencv/opencv_contrib/archive/3.2.0.zip
RUN unzip opencv.zip \
    && unzip opencv_contrib.zip
RUN cd opencv-3.2.0 \
    && mkdir build \
    && cd build \
    && cmake -D CMAKE_BUILD_TYPE=RELEASE \
        -D CMAKE_INSTALL_PREFIX=/usr/local \
        -D INSTALL_PYTHON_EXAMPLES=ON \
        -D INSTALL_C_EXAMPLES=OFF \
        -D OPENCV_EXTRA_MODULES_PATH=/code/opencv_contrib-3.2.0/modules \
        -D PYTHON_EXECUTABLE=/usr/bin/python3.5
        -D BUILD_EXAMPLES=ON .. \
    && make -j4 \
    && make install && ldconfig
RUN cd /code \
    && rm -rf opencv-3.2.0 opencv_contrib-3.2.0 opencv.zip opencv_contrib.zip get-pip.py

# Build mjpg_streamer
# Need to run container with the device: docker run -t -i -p 8080:8080/tcp --device=/dev/video0 image:tag "-i input_opencv.so"
RUN git clone https://github.com/chunliu/mjpg-streamer.git 

WORKDIR mjpg-streamer/mjpg-streamer-experimental

RUN make \ 
    && make install \

EXPOSE 8080/TCP

ENTRYPOINT ["docker-start.sh", "-o output_http.so -w ./www"]

CMD ["-i input_uvc.so"]

# To use OpenCV input: 
# CMD ["mjpg_streamer", "-o output_http.so -w ./www", "-i input_opencv.so"]
