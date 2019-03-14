.PHONY: clean

clean:
	rm -f *.o *~ core sr *.dump *.tar tags mklfs lfs file

CC = gcc

ifeq ($(OSTYPE),Linux)
ARCH = -D_LINUX_
SOCK = -lnsl
endif

ifeq ($(OSTYPE),SunOS)
ARCH =  -D_SOLARIS_
SOCK = -lnsl -lsocket
endif

ifeq ($(OSTYPE),Darwin)
ARCH = -D_DARWIN_
SOCK =
endif

CFLAGS = -g -Wall -std=gnu99 -D_DEBUG_ -DVNL $(ARCH)

lfs_SRCS = flash.c log.c lfs.c file.c
lfs_OBJS = $(patsubst %.c,%.o,$(lfs_SRCS))
lfs_DEPS = $(patsubst %.c,.%.d,$(lfs_SRCS))

mklfs_SRCS = flash.c mklfs.c

$(lfs_OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(lfs_DEPS) : .%.d : %.c
	$(CC) -MM $(CFLAGS) $<  > $@

include $(lfs_DEPS)	

lfs : $(lfs_OBJS)
	$(CC) $(CFLAGS) -o lfs $(lfs_OBJS) 

mklfs : $(mklfs_OBJS)
	$(CC) $(CFLAGS) -o mklfs $(mklfs_SRCS) 