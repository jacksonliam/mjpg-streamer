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
LFLAGS += 

all: input_file.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_file.so: $(OTHER_HEADERS) input_file.c
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ input_file.c
