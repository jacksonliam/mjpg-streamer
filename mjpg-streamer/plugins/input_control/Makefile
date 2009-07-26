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
#CFLAGS += -DDEBUG

all: input_control.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_control.so: $(OTHER_HEADERS) input_uvc.c dynctrl.lo
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ input_uvc.c dynctrl.lo

dynctrl.lo: dynctrl.c dynctrl.h uvcvideo.h
	$(CC) -c $(CFLAGS) -o $@ dynctrl.c
