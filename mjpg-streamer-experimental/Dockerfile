FROM chunliu/docker-opencv

MAINTAINER Chun Liu <https://github.com/chunliu>
LABEL Description="A Docker image for mjpg_streamer." Version="0.3"

# Build mjpg_streamer
# Need to run container with the device: docker run -t -i -p 8080:8080/tcp --device=/dev/video0 image:tag
RUN git clone https://github.com/jacksonliam/mjpg-streamer.git 

WORKDIR /mjpg-streamer/mjpg-streamer-experimental

RUN make \ 
    && make install \
    && chmod +x docker-start.sh

EXPOSE 8080/TCP

ENTRYPOINT ["/mjpg-streamer/mjpg-streamer-experimental/docker-start.sh", "output_http.so -w ./www"]

CMD ["input_uvc.so"]
