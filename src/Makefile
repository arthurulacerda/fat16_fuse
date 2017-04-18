CFLAGS=$(shell pkg-config fuse --cflags)
LIBS=$(shell pkg-config fuse --libs)

CC=clang

all: mount_fat16

mount_fat16: mount_fat16.o sector.o log.o
	$(CC) -o $@ $^ $(LIBS)

mount_fat16.o: mount_fat16.c

sector.o: sector.c sector.h

log.o: log.c log.h

clean:
	rm -f mount_fat16 *.o
