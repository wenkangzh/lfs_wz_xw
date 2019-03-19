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
#include "lfs.h"

struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;

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

void print_inode(struct inode *inode)
{
	printf("\t----- inode -----\n");
	printf("\t inum: %u\n", inode->inum);
	printf("\t type: %u\n", inode->type);
	printf("\t size: %u\n", inode->size);
	printf("\t addr0: %u %u\n", inode->ptrs[0].seg_num, inode->ptrs[0].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[1].seg_num, inode->ptrs[1].block_num);
	printf("\t addr2: %u %u\n", inode->ptrs[2].seg_num, inode->ptrs[2].block_num);
	printf("\t addr3: %u %u\n", inode->ptrs[3].seg_num, inode->ptrs[3].block_num);
	printf("\t-----------------\n");
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

int main(int argc, char *argv[])
{
	char *flash_file_name = argv[argc-1];
	u_int *n_blocks = malloc(sizeof(u_int));
	Flash flash = Flash_Open(flash_file_name, FLASH_ASYNC, n_blocks);
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

	print_superblock(lfs_sb);

	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;

	cp_region = malloc(sizeof(struct checkpoint_region));

	// Log_Read(&(lfs_sb->checkpoint_addr), sizeof(struct checkpoint_region), cp_region);

	u_int sector_offset = logAddr_To_Sectors(&(lfs_sb->checkpoint_addr));
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = sizeof(struct checkpoint_region) / FLASH_SECTOR_SIZE + (sizeof(struct checkpoint_region) % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp_t = malloc(FLASH_SECTOR_SIZE * sector_n);
	if(Flash_Read(flash, sector_offset, sector_n, temp_t) == 1)
		printf("ERROR: %s\n", strerror(errno));
	memcpy(cp_region, temp_t, sizeof(struct checkpoint_region));

	print_cp_region();


	// // now traverse root directory.
	// struct inode *root_inode = malloc(sizeof(struct inode));
	// Read_Inode_in_Ifile(LFS_ROOT_INUM, root_inode);
	// void *root_buffer = malloc(root_inode->size);
	// File_Read(LFS_ROOT_INUM, 0, root_inode->size, root_buffer);
	// struct dir *dir_hdr = root_buffer;
	// printf("------------ %s ------------\n", dir_hdr->name);
	// struct dir_entry *entry_ptr = root_buffer + sizeof(struct dir);
	// for (int i = 0; i < dir_hdr->size; ++i)
	// {
	// 	entry_ptr = root_buffer + sizeof(struct dir) + sizeof(struct dir_entry) * i;
	// 	printf("\t\t -- %s : %u --\n", entry_ptr->name, entry_ptr->inum);
	// }
	// printf("----------------------------\n");

	return 0;
}