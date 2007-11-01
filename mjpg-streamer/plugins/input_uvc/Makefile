###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

CC = gcc

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
LFLAGS += -lpthread -ldl

all world: input_uvc.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_uvc.so: ../input.h input_uvc.c v4l2uvc.lo
	$(CC) $(CFLAGS) -o $@ input_uvc.c v4l2uvc.lo

.SUFFIXES: .c .lo
.c.lo:
	$(CC) -c $(CFLAGS) -o $@ $<