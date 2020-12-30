FROM debian:stable-slim
# Build mjpg_streamer
# Need to run container with the device: docker run -t -i -p 8080:8080/tcp --device=/dev/video0 image:tag

LABEL Description="A Docker image for mjpg_streamer." Version="0.4"

RUN apt-get update \
    && apt-get install -y --no-install-recommends  \
        build-essential     \
        ca-certificates     \
        cmake               \
        g++                 \
        git-core            \
        libjpeg-dev         \
        && \
    rm -rf /src/* /tmp/* /var/lib/apt/lists/*

RUN git clone https://github.com/davralin/mjpg-streamer.git /usr/local/src/

RUN cd /usr/local/src/mjpg-streamer-experimental \
    && make \
    && make install \
    && chmod +x docker-start.sh

EXPOSE 8080/TCP

WORKDIR /usr/local/src/mjpg-streamer-experimental

ENTRYPOINT ["/usr/local/src/mjpg-streamer-experimental/docker-start.sh", "output_http.so -w ./www"]

CMD ["input_uvc.so"]
