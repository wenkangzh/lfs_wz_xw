.PHONY: clean

clean:
	rm -f *.o *~ core sr *.dump *.tar tags mklfs lfs file lfsck

CC = gcc

FUSE_FLAGS = `pkg-config fuse --cflags --libs`
CFLAGS = -g -Wall -std=gnu99 -D_DEBUG_ 

lfs_SRCS = flash.c log.c lfs.c file.c

mklfs_SRCS = flash.c mklfs.c

lfsck_SRCS = flash.c lfsck.c

lfs : $(lfs_SRCS)
	$(CC) $(CFLAGS) $(lfs_SRCS) $(FUSE_FLAGS) -o lfs

mklfs : $(mklfs_SRCS)
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o mklfs $(mklfs_SRCS) 

lfsck : $(lfsck_SRCS)
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -o lfsck $(lfsck_SRCS)