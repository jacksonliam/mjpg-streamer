###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

CC = gcc

OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
#CFLAGS += -g
#CFLAGS += -DDEBUG -g
LFLAGS += -lpthread -ldl -lprotobuf-c -lzmq

all: proto output_zmqserver.so

proto:
	protoc-c package.proto --c_out=./

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_zmqserver.so: $(OTHER_HEADERS) package.pb-c.c output_zmqserver.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ package.pb-c.c output_zmqserver.c
