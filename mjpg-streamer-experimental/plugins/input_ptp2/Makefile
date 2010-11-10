CC = gcc

OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

CFLAGS += -O0 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC -g
#CFLAGS += -DDEBUG
LFLAGS += -lpthread -ldl -lgphoto2

all: input_ptp2.so

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_ptp2.so: $(OTHER_HEADERS) input_ptp2.c input_ptp2.h
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ input_ptp2.c
