###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.4
# License: GPL
#
###############################################################

# specifies where to install the binaries after compilation
# to use another directory you can specify it with:
# $ sudo make DESTDIR=/some/path install
DESTDIR = /usr/local

# set the compiler to use
CC = gcc

SVNDEV := -D'SVN_REV="$(shell svnversion -c .)"'
CFLAGS += $(SVNDEV)

# general compile flags, enable all warnings to make compile more verbose
CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall 
# CFLAGS += -g 
#CFLAGS +=  -DDEBUG

# we are using the libraries "libpthread" and "libdl"
# libpthread is used to run several tasks (virtually) in parallel
# libdl is used to load the plugins (shared objects) at runtime
LFLAGS += -lpthread -ldl

# define the name of the program
APP_BINARY = mjpg_streamer

# define the names and targets of the plugins
PLUGINS = input_uvc.so
PLUGINS += output_file.so
PLUGINS += output_udp.so
PLUGINS += output_http.so
PLUGINS += input_testpicture.so
#PLUGINS += output_autofocus.so
#PLUGINS += input_gspcav1.so
PLUGINS += input_file.so
# PLUGINS += output_rtsp.so
# PLUGINS += output_ptp2.so # commented out because it depends on libgphoto
# PLUGINS += input_control.so # commented out because the output_http does it's job
# PLUGINS += input_http.so 
# PLUGINS += output_viewer.so # commented out because it depends on SDL

# define the names of object files
OBJECTS=mjpg_streamer.o utils.o

# this is the first target, thus it will be used implictely if no other target
# was given. It defines that it is dependent on the application target and
# the plugins
all: application plugins

application: $(APP_BINARY)

plugins: $(PLUGINS)

$(APP_BINARY): mjpg_streamer.c mjpg_streamer.h mjpg_streamer.o utils.c utils.h utils.o
	$(CC) $(CFLAGS) $(OBJECTS) $(LFLAGS) -o $(APP_BINARY)
	chmod 755 $(APP_BINARY)

output_autofocus.so: mjpg_streamer.h utils.h
	make -C plugins/output_autofocus all
	cp plugins/output_autofocus/output_autofocus.so .

input_testpicture.so: mjpg_streamer.h utils.h
	make -C plugins/input_testpicture all
	cp plugins/input_testpicture/input_testpicture.so .


ifeq ($(USE_LIBV4L2),true)
input_uvc.so: mjpg_streamer.h utils.h
	make -C plugins/input_uvc USE_LIBV4L2=true all
	cp plugins/input_uvc/input_uvc.so .
else
input_uvc.so: mjpg_streamer.h utils.h
	make -C plugins/input_uvc all
	cp plugins/input_uvc/input_uvc.so .
endif

input_control.so: mjpg_streamer.h utils.h
	make -C plugins/input_control all
	cp plugins/input_control/input_control.so .

output_file.so: mjpg_streamer.h utils.h
	make -C plugins/output_file all
	cp plugins/output_file/output_file.so .

ifeq ($(WXP_COMPAT),true)
output_http.so: mjpg_streamer.h utils.h
	make -C plugins/output_http -DWXP_COMPAT all
	cp plugins/output_http/output_http.so .
else
output_http.so: mjpg_streamer.h utils.h
	make -C plugins/output_http all
	cp plugins/output_http/output_http.so .
endif

output_udp.so: mjpg_streamer.h utils.h
	make -C plugins/output_udp all
	cp plugins/output_udp/output_udp.so .

input_gspcav1.so: mjpg_streamer.h utils.h
	make -C plugins/input_gspcav1 all
	cp plugins/input_gspcav1/input_gspcav1.so .

input_file.so: mjpg_streamer.h utils.h
	make -C plugins/input_file all
	cp plugins/input_file/input_file.so .

output_rtsp.so: mjpg_streamer.h utils.h
	make -C plugins/output_rtsp all
	cp plugins/output_rtsp/output_rtsp.so .
	
output_ptp2.so: mjpg_streamer.h utils.h
	make -C plugins/input_ptp2 all
	cp plugins/input_ptp2/input_ptp2.so .	

#input_http.so: mjpg_streamer.h utils.h
#	make -C plugins/input_http all
#	cp plugins/input_http/input_http.so .

# The viewer plugin requires the SDL library for compilation
# This is very uncommmon on embedded devices, so it is commented out and will
# not be build automatically. If you compile for PC, install libsdl and then
# execute the following command:
# make output_viewer.so
output_viewer.so: mjpg_streamer.h utils.h
	make -C plugins/output_viewer all
	cp plugins/output_viewer/output_viewer.so .

# cleanup
clean:
	make -C plugins/input_uvc $@
	make -C plugins/input_testpicture $@
	make -C plugins/output_file $@
	make -C plugins/output_http $@
	make -C plugins/output_udp $@
	make -C plugins/output_autofocus $@
	make -C plugins/input_gspcav1 $@
	make -C plugins/output_viewer $@
	make -C plugins/input_control $@
	make -C plugins/output_rtsp $@
#	make -C plugins/input_http $@
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo

# useful to make a backup "make tgz"
tgz: clean
	mkdir -p backups
	tar czvf ./backups/mjpg_streamer_`date +"%Y_%m_%d_%H.%M.%S"`.tgz --exclude backups --exclude .svn *

# install MJPG-streamer and example webpages
install: all
	install --mode=755 $(APP_BINARY) $(DESTDIR)/bin
	install --mode=644 $(PLUGINS) $(DESTDIR)/lib/
	install --mode=755 -d $(DESTDIR)/www
	install --mode=644 -D www/* $(DESTDIR)/www

# remove the files installed above
uninstall:
	rm -f $(DESTDIR)/bin/$(APP_BINARY)
	for plug in $(PLUGINS); do \
	  rm -f $(DESTDIR)/lib/$$plug; \
	done;
