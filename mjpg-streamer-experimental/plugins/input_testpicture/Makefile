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

all: input_testpicture.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

# pictures will not get cleaned automatically, it may be possible
# that other users do not have the required tools for re-building installed
# required tools are:
#  * convert (ImageMagick)
#  * od, sed, tr
clean_pictures:
	rm -f testpictures.h
	rm -f pictures/160x120_1.jpg pictures/160x120_2.jpg
	rm -f pictures/320x240_1.jpg pictures/320x240_2.jpg
	rm -f pictures/640x480_1.jpg pictures/640x480_2.jpg

input_testpicture.so: $(OTHER_HEADERS) input_testpicture.c testpictures.h
	$(CC) $(CFLAGS) -o $@ input_testpicture.c

# converts multiple JPG files to a single C header file
testpictures.h: pictures/960x720_1.jpg pictures/640x480_1.jpg pictures/320x240_1.jpg pictures/160x120_1.jpg pictures/160x120_2.jpg pictures/320x240_2.jpg pictures/640x480_2.jpg pictures/960x720_2.jpg
	@echo "/* automatically generated content, do not edit manually!" > testpictures.h.tmp
	@echo " */ " >> testpictures.h.tmp
	@FILES="160x120_1 320x240_1 640x480_1 960x720_1 160x120_2 320x240_2 640x480_2 960x720_2" && \
	  for file in $$FILES; do \
	    echo "converting file: $$file.jpg"; \
	    echo "static const unsigned char PIC_$$file[] = { " >> testpictures.h.tmp; \
	    od -An -v -t x1 pictures/$$file.jpg | sed -e "s/\ \([0-9a-fA-F]*\)/\ 0x\1,/g" >> testpictures.h.tmp; \
	    echo "};" >> testpictures.h.tmp; \
	  done;
	@cat testpictures.h.tmp | tr '\n' 'N' | sed -e "s/,N};/};/g" | tr 'N' '\n' > testpictures.h
	@rm testpictures.h.tmp

pictures/160x120_1.jpg: pictures/960x720_1.jpg
	convert pictures/960x720_1.jpg -resize 160x120! pictures/160x120_1.jpg

pictures/160x120_2.jpg: pictures/960x720_2.jpg
	convert pictures/960x720_2.jpg -resize 160x120! pictures/160x120_2.jpg

pictures/320x240_1.jpg: pictures/960x720_1.jpg
	convert pictures/960x720_1.jpg -resize 320x240! pictures/320x240_1.jpg

pictures/320x240_2.jpg: pictures/960x720_2.jpg
	convert pictures/960x720_2.jpg -resize 320x240! pictures/320x240_2.jpg

pictures/640x480_1.jpg: pictures/960x720_1.jpg
	convert pictures/960x720_1.jpg -resize 640x480! pictures/640x480_1.jpg

pictures/640x480_2.jpg: pictures/960x720_2.jpg
	convert pictures/960x720_2.jpg -resize 640x480! pictures/640x480_2.jpg
