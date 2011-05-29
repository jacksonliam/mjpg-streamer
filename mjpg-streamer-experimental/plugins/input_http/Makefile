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

HEADERS = misc.h mjpg-proxy.h version.h
SOURCES = input_http.c misc.c  mjpg-proxy.c
FILES =  $(SOURCES) $(HEADERS)

all: input_http.so

clean:
	rm -f *.a *.o core *~ *.so *.lo


input_http.so: $(OTHER_HEADERS) $(FILES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)
	
zip:
	zip -9 input_http.zip $(FILES) Makefile

.PHONY: zip 

