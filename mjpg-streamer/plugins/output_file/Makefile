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

all world: output_file.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_file.so: ../output.h ../input.h output_file.c
	$(CC) $(CFLAGS) -o $@ output_file.c

.SUFFIXES: .c .lo
.c.lo:
	$(CC) -c $(CFLAGS) -o $@ $<