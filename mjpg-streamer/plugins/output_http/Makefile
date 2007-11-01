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

all world: output_http.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_http.so: ../output.h ../input.h output_http.c httpd.h httpd.lo
	$(CC) $(CFLAGS) -o $@ output_http.c httpd.lo

.SUFFIXES: .c .lo
.c.lo:
	$(CC) -c $(CFLAGS) -o $@ $<
