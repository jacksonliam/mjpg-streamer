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
LFLAGS += -lpthread -ldl

all: input_gspcav1.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_gspcav1.so: $(OTHER_HEADERS) input_gspcav1.c spcav4l.lo utils.lo encoder.lo huffman.lo marker.lo quant.lo
	$(CC) $(CFLAGS) -o $@ input_gspcav1.c spcav4l.lo utils.lo encoder.lo huffman.lo marker.lo quant.lo

spcav4l.lo:	spcav4l.c spcav4l.h
	$(CC) -c $(CFLAGS) -o $@ $<
		
utils.lo:	utils.c utils.h
	$(CC) -c $(CFLAGS) -o $@ $<

encoder.lo:	encoder.c encoder.h
	$(CC) -c $(CFLAGS) -o $@ $<

huffman.lo:	huffman.c huffman.h
	$(CC) -c $(CFLAGS) -o $@ $<

marker.lo:	marker.c marker.h
	$(CC) -c $(CFLAGS) -o $@ $<

quant.lo:	quant.c quant.h
	$(CC) -c $(CFLAGS) -o $@ $<
