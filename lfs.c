#include "lfs.h"
#include "log.h"
#include "flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;

void init()
{
	u_int *blocks_n = malloc(sizeof(u_int));
	flash = Flash_Open("file", FLASH_ASYNC, blocks_n);
	init_sb();

	// Compute size of a block in Byte
	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;

	// TODO - need to restore information in checkpoint region in memory.
	cp_region = malloc(sizeof(struct checkpoint_region));

	// Log_Read(&(lfs_sb->checkpoint_addr), sizeof(struct checkpoint_region), cp_region);

	u_int sector_offset = logAddr_To_Sectors(&(lfs_sb->checkpoint_addr));
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = sizeof(struct checkpoint_region) / FLASH_SECTOR_SIZE + (sizeof(struct checkpoint_region) % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp = malloc(FLASH_SECTOR_SIZE * sector_n);
	if(Flash_Read(flash, sector_offset, sector_n, temp) == 1)
		printf("ERROR: %s\n", strerror(errno));
	memcpy(cp_region, temp, sizeof(struct checkpoint_region));

	print_cp_region();

	tail_seg = malloc(sizeof(struct segment));
	tail_seg->seg_num = cp_region == NULL ? LFS_SEG(0) : cp_region->last_seg_addr.seg_num + 1; // TODO find next availble segment
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
	printf("CK_addr_seg: %u\n", lfs_sb->checkpoint_addr.seg_num);
	printf("CK_addr_blo: %u\n", lfs_sb->checkpoint_addr.block_num);
}

void print_cp_region()
{
	printf("---------- CP region ----------\n");
	printf("timestamp: %ld\n", cp_region->timestamp);
	printf("LSA-seg_num: %u\n", cp_region->last_seg_addr.seg_num);
	printf("LSA-block: %u\n", cp_region->last_seg_addr.block_num);
	printf("ifile-inode-inum: %u\n", cp_region->ifile_inode.inum);
	printf("ifile-inode-type: %u\n", cp_region->ifile_inode.type);
	printf("ifile-inode-size: %d\n", cp_region->ifile_inode.size);
	printf("ifile-inode-addr-seg: %u\n", cp_region->ifile_inode.ptrs[0].seg_num);
	printf("ifile-inode-addr-block: %u\n", cp_region->ifile_inode.ptrs[0].block_num);
	printf("-------------------------------\n");
}

void print_inode(struct inode *inode)
{
	printf("\t----- inode -----\n");
	printf("\t inum: %u\n", inode->inum);
	printf("\t type: %u\n", inode->type);
	printf("\t size: %u\n", inode->size);
	printf("\t addr0: %u %u\n", inode->ptrs[0].seg_num, inode->ptrs[0].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[1].seg_num, inode->ptrs[1].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[2].seg_num, inode->ptrs[2].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[3].seg_num, inode->ptrs[3].block_num);
	printf("\t-----------------\n");
}


int main(int argc, char const *argv[])
{
	init();
	print_sb();
	

	for (int i = 0; i < 40; ++i)
	{
		File_Create(i+1, LFS_FILE_TYPE_FILE);
	}

	for (int i = 0; i < 40; ++i)
	{
		char a = (char) i+48;
		File_Write(i+1, 0, 1, &a);
	}

	for (int i = 0; i < 40; ++i)
	{
		void *result = malloc(2);
		File_Read(i+1, 0, 1, result);
		printf("RESULT: %s\n", (char *) result);
	}

	print_cp_region();

	if (Flash_Close(flash) != 0) {
		printf("ERROR: %s\n", strerror(errno));
	}
	return 0;
}