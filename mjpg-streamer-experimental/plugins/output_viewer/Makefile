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
CFLAGS += -DUSE_SDL -I/usr/include/SDL
#CFLAGS += -DDEBUG

LFLAGS += -ljpeg -lSDL

all: output_viewer.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_viewer.so: $(OTHER_HEADERS) output_viewer.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ output_viewer.c
