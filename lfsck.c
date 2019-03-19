#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "flash.h"
#include <errno.h>
#include "lfs.h"
#include "log.h"
#include "file.h"
#include <time.h>

void print_superblock(struct superblock *sb)
{
	printf("--------------------- SB -------------------\n");
	printf("seg_size: %u\n", sb->seg_size);
	printf("b_size: %u\n", sb->b_size);
	printf("seg_num: %u\n", sb->seg_num);
	printf("sb_seg_num: %u\n", sb->sb_seg_num);
	printf("CK_addr_seg: %u\n", sb->checkpoint_addr.seg_num);
	printf("CK_addr_blo: %u\n", sb->checkpoint_addr.block_num);
	printf("--------------------------------------------\n");
}

int main(int argc, char *argv[])
{
	char *flash_file_name = argv[argc-1];
	u_int *n_blocks = malloc(sizeof(u_int));
	Flash flash = Flash_Open(flash_file_name, FLASH_ASYNC, n_blocks);
	struct superblock *sb = malloc(sizeof(struct superblock));

	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	if(Flash_Read(flash, 0, 64, buffer) == 1)
		printf("ERROR: %s\n", strerror(errno));
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer + sizeof(uint16_t));
	uint16_t *sb_seg_num = (uint16_t*) (buffer + sizeof(uint16_t) * 3);
	int sectors = *seg_size * *b_size * *sb_seg_num;
	void *temp = malloc(sectors * FLASH_SECTOR_SIZE);
	Flash_Read(flash, 0, sectors, temp);
	memcpy(sb, temp, sizeof(struct superblock));

	print_superblock(sb);

	return 0;
}