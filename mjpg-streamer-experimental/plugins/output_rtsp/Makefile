###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

# Modified to work with the output_udp plugin by D.Zachariadis

CC = gcc

OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
#CFLAGS += -DDEBUG
LFLAGS += -lpthread -ldl

all: output_rtsp.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_rtsp.so: $(OTHER_HEADERS) output_rtsp.c
	$(CC) $(CFLAGS) -o $@ output_rtsp.c
