FROM chunliu/docker-opencv

# Build mjpg_streamer
# Need to run container with the device: docker run -t -i -p 8080:8080/tcp --device=/dev/video0 image:tag "-i input_opencv.so"
RUN git clone https://github.com/chunliu/mjpg-streamer.git 

WORKDIR /mjpg-streamer/mjpg-streamer-experimental

RUN make \ 
    && make install \
    && chmod +x docker-start.sh

EXPOSE 8080/TCP

ENTRYPOINT ["/mjpg-streamer/mjpg-streamer-experimental/docker-start.sh", "-o output_http.so -w ./www"]

CMD ["-i input_uvc.so"]
