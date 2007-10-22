###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.2
# License: GPL
#
###############################################################

CC=gcc
APP_BINARY=mjpg_streamer

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall
#CFLAGS += -O2 -DDEBUG -DLINUX -D_GNU_SOURCE -Wall
LFLAGS += -lpthread -ldl

OBJECTS=mjpg_streamer.o utils.o

all: uga_buga

clean:
	@echo "Cleaning up directory."
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo
	rm -f plugins/*/*.a plugins/*/*.o plugins/*/core plugins/*/*~ plugins/*/*.so plugins/*/*.lo

# Applications:
uga_buga: $(OBJECTS) input_uvc.so output_http.so output_file.so
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o $(APP_BINARY)
	chmod 755 $(APP_BINARY)

#Plugins
input_uvc.so: plugins/input_uvc/input_uvc.c plugins/input.h plugins/input_uvc/v4l2uvc.lo
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< plugins/input_uvc/v4l2uvc.lo

output_http.so: plugins/output_http/output_http.c plugins/output_http/httpd.lo plugins/output_http/httpd.h plugins/output.h
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< plugins/output_http/httpd.lo

output_file.so: plugins/output_file/output_file.c plugins/output.h
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $<

# useful to make a backup "make tgz"
tgz: clean
	mkdir -p backups
	tar czvf ./backups/mjpg_streamer_`date +"%Y_%m_%d_%H.%M.%S"`.tgz --exclude backups *

.SUFFIXES: .c .lo
.c.lo:
	$(CC) -c -shared -fPIC $(CFLAGS) -o $@ $<

