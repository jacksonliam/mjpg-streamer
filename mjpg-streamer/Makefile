###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

CC = gcc

APP_BINARY=mjpg_streamer

CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall
#CFLAGS += -O2 -DDEBUG -DLINUX -D_GNU_SOURCE -Wall
LFLAGS += -lpthread -ldl

OBJECTS=mjpg_streamer.o utils.o

all: world

clean:
	make -C plugins/input_uvc $@
	make -C plugins/output_file $@
	make -C plugins/output_http $@
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo

compile_plugins:
	make -C plugins/input_uvc all
	cp plugins/input_uvc/input_uvc.so .
	make -C plugins/output_file all
	cp plugins/output_file/output_file.so .
	make -C plugins/output_http all
	cp plugins/output_http/output_http.so .

world: compile_plugins $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o $(APP_BINARY)
	chmod 755 $(APP_BINARY)

# useful to make a backup "make tgz"
tgz: clean
	mkdir -p backups
	tar czvf ./backups/mjpg_streamer_`date +"%Y_%m_%d_%H.%M.%S"`.tgz --exclude backups *
