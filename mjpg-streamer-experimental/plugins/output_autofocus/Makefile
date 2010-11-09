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

#CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
CFLAGS += -DDEBUG -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
LFLAGS += -lpthread -ldl

all: output_autofocus.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_autofocus.so: $(OTHER_HEADERS) output_autofocus.c processJPEG_onlyCenter.lo
	$(CC) $(CFLAGS) -lm -o $@ output_autofocus.c processJPEG_onlyCenter.lo

processJPEG_onlyCenter.lo: $(OTHER_HEADERS) processJPEG_onlyCenter.h
	$(CC) -c $(CFLAGS) -o $@ processJPEG_onlyCenter.c
