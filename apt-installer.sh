apt install git cmake libjpeg8-dev gcc g++ -y
git clone https://github.com/jacksonliam/mjpg-streamer.git
cd mjpg-streamer/mjpg-streamer-experimental
make
make install
cd ../..
rm -rf mjpg-streamer
