CC ?= gcc

all: libppool.so libppool.a example

libppool.so:
	$(CC) -shared -fPIC ppool.c ppool_queue.c ppool_errno.c -o libppool.so -pthread

libppool.a:
	$(CC) -O2 -c ppool.c
	$(CC) -O2 -c ppool_queue.c
	$(CC) -O2 -c ppool_errno.c
	ar rcs libppool.a ppool.o ppool_queue.o ppool_errno.o

example:
	$(CC) -g example.c ppool.c ppool_queue.c ppool_errno.c -o example -pthread

clean:
	rm -rfv *.o *.a *.so example