#include "lfs.h"
#include "log.h"
#include "flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;

void init()
{
	u_int *blocks_n = malloc(sizeof(u_int));
	flash = Flash_Open("file", FLASH_ASYNC, blocks_n);
	init_sb();

	// TODO - need to restore information in checkpoint region in memory.
	// cp_region = malloc(sizeof(struct checkpoint_region));
	// Log_Read(&(lfs_sb->checkpoint_addr), sizeof(struct checkpoint_region), cp_region);



	tail_seg = malloc(sizeof(struct segment));
	tail_seg->seg_num = cp_region == NULL ? 0 : cp_region->last_seg_addr.seg_num + 1; // TODO find next availble segment
	tail_seg->blocks = malloc(lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	memset(tail_seg->blocks, 0, lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE); // clean the memory
	// here the blocks in the tail segment are all allocated memory space.
}

void init_sb()
{
	lfs_sb = malloc(sizeof(struct superblock));

	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	if(Flash_Read(flash, 0, 64, buffer) == 1)
		printf("ERROR: %s\n", strerror(errno));
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer + sizeof(uint16_t));
	uint16_t *sb_seg_num = (uint16_t*) (buffer + sizeof(uint16_t) * 3);
	int sectors = *seg_size * *b_size * *sb_seg_num;
	void *temp = malloc(sectors * FLASH_SECTOR_SIZE);
	Flash_Read(flash, 0, sectors, temp);
	memcpy(lfs_sb, temp, sizeof(struct superblock));
}

void print_sb()
{
	printf("seg_size: %u\n", lfs_sb->seg_size);
	printf("b_size: %u\n", lfs_sb->b_size);
	printf("seg_num: %u\n", lfs_sb->seg_num);
	printf("sb_seg_num: %u\n", lfs_sb->sb_seg_num);
	printf("time0: %ld\n", lfs_sb->ck_region_0.timestamp);
	printf("time1: %ld\n", lfs_sb->ck_region_1.timestamp);
}


int main(int argc, char const *argv[])
{
	init();
	// print_sb();
	char *buffer = (char *) malloc(sizeof(char) * 1000);
	memset(buffer, 'A', sizeof(char) * 1000);

	struct addr *logAddress = malloc(sizeof(struct addr));
	Log_Write(1, 1, sizeof(char) * 1000, buffer, &logAddress);
	// 
	void *result = malloc(sizeof(char) * 1000);
	Log_Read(logAddress, sizeof(char) * 1000, result);
	printf("%s\n", (char *) result);



	return 0;
}