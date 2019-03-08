#include "lfs.h"
#include "log.h"
#include "flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

int main(int argc, char const *argv[])
{
	u_int *blocks_n = malloc(sizeof(u_int));
	flash = Flash_Open("file", FLASH_ASYNC, blocks_n);
	// struct superblock *sb = malloc(sizeof(struct superblock));
	// get_superblock(sb);
	// printf("%u\n", sb->seg_size);
	// printf("%u\n", sb->b_size);
	// printf("%u\n", sb->seg_num);

	char *buffer = (char *) malloc(sizeof(char) * 1024);
	memset(buffer, 'A', sizeof(char) * 1024);

	int i = Log_Write(1, 1, sizeof(char) * 1024, buffer, NULL);
	if(i == 1)
		printf("ERROR: %s\n", strerror(errno));

	return 0;
}