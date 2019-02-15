.PHONY: clean
CC = gcc
CFLAGS = -Wall -g
CFILES = flash.c mklfs.c
DEST = mklfs

$(DEST) : mklfs.c
	$(CC) $(CFLAGS) $(CFILES) -o $(DEST)

clean:
	/bin/rm -f -r $(DEST) *.gcov *.gcno *.gcda *.gch *.dSYM