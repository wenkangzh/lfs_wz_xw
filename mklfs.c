/**
*	filename: mklfs.c
*	CSC 552 LFS Project Phase 1
*	Wenkang Zhou wenkangzhou@email.arizona.edu
*	Xueheng Wan  wanxueheng@email.arizona.edu
*/

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

int main(int argc, char *argv[])
{
	int sizeof_block = 2; 		// size of a block, in sectors
	int sizeof_segment = 32;  	// segment size, in blocks, must be multiple of flash erase block size
	int sizeof_flash = 100; 	// size of flash, in segments.
	int wearlimit = 1000;		// wear limit for erase blocks
	char *filename = argv[argc-1];

	int opt = 0;
	struct option long_options[] = {
		{"block", 		required_argument, 0, 'b'}, 
		{"segment", 	required_argument, 0, 'l'},
		{"segments", 	required_argument, 0, 's'},
		{"wearlimit", 	required_argument, 0, 'w'}
	};
	int long_index = 0;
	while ((opt = getopt_long(argc, argv, "b:l:s:w:", 
					long_options, &long_index)) != -1){
		switch (opt) {
			case 'b':
				sizeof_block = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'l':
				sizeof_segment = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 's':
				sizeof_flash = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'w':
				wearlimit = (int) strtol(optarg, (char **)NULL, 10);
				break;
			default:
				printf("usage: ... \n");
		}
	}

	printf("size of block: %d sectors\n", sizeof_block);
	printf("size of segment: %d blocks\n", sizeof_segment);
	printf("size of flash: %d segments\n", sizeof_flash);
	printf("wear limit: %d blocks\n", wearlimit);
	printf("Filename: %s\n", filename);

	int sizeof_flash_in_blocks = sizeof_flash * sizeof_segment;
	if(Flash_Create(filename, wearlimit, sizeof_flash_in_blocks) != 0){
		printf("ERROR: %s\n", strerror(errno));
	}
	// create done. 
	// now do initialization of the LFS
	// 	num_of_block_superblock represents number of blocks being reserved for super block excepting for segment usage table
	int num_of_block_superblock = sizeof(struct superblock) / (sizeof_block * FLASH_SECTOR_SIZE);
	if(sizeof(struct superblock) % (sizeof_block * FLASH_SECTOR_SIZE) != 0) num_of_block_superblock ++;
	// 	num_of_block_seg_usage_table represents number of blocks being reserved for segment usage table
	int num_of_block_seg_usage_table = sizeof_flash * sizeof(uint8_t) / (sizeof_block * FLASH_SECTOR_SIZE);
	if(sizeof_flash * sizeof(uint8_t) % (sizeof_block * FLASH_SECTOR_SIZE) != 0) num_of_block_seg_usage_table ++;
	//	sb_seg_size represents number of segment super block overall used
	int sb_seg_size = (num_of_block_superblock + num_of_block_seg_usage_table) / (sizeof_segment);
	if((num_of_block_superblock + num_of_block_seg_usage_table) % (sizeof_segment) != 0) sb_seg_size ++;

	void *sb_buffer = malloc(sizeof_block * sizeof_segment * sb_seg_size * FLASH_SECTOR_SIZE);
	struct superblock *superblock = sb_buffer;
	superblock->seg_size = sizeof_segment;
	superblock->b_size = sizeof_block;
	superblock->seg_num = sizeof_flash;
	superblock->sb_seg_num = sb_seg_size;
	superblock->checkpoint_addr.seg_num = sb_seg_size;
	superblock->checkpoint_addr.block_num = 2;		// checkpoint region is in the third block
	superblock->seg_usage_table_addr.seg_num = num_of_block_superblock / sizeof_segment + (num_of_block_superblock % sizeof_segment == 0 ? 1 : 0);
	superblock->seg_usage_table_addr.block_num = num_of_block_superblock % sizeof_segment;

	// write the superblock in flash.
	u_int *n_blocks = malloc(sizeof(u_int));
	Flash flash = Flash_Open(filename, FLASH_ASYNC, n_blocks);
	printf("%u\n", *n_blocks);

	// write segment usage table
	void *sut = sb_buffer + num_of_block_superblock * sizeof_block * FLASH_SECTOR_SIZE;
	memset(sut, 0, sizeof(sut));
	memset(sut, 1, sizeof(uint8_t) * (sb_seg_size + 1));

	// Write superblock to flash
	Flash_Write(flash, 0, sizeof_block * sizeof_segment * sb_seg_size, sb_buffer);

	// TODO the initial ifile. Empty? Could use File_Create()? 
	void *ifile_buffer = malloc(sizeof_block * FLASH_SECTOR_SIZE);		// 1 block
	struct inode *root_inode = malloc(sizeof(struct inode));
	root_inode->inum = LFS_ROOT_INUM;
	root_inode->type = LFS_FILE_TYPE_DIR;
	root_inode->size = sizeof(struct dir_entry) * 3 + sizeof(struct dir);		// initially there are ".", "..", ".ifile" in the root
	root_inode->ptrs[0].seg_num = sb_seg_size;
	root_inode->ptrs[0].block_num = 1;
	root_inode->ptrs[1].seg_num = LFS_UNUSED_ADDR;
	root_inode->ptrs[1].block_num = LFS_UNUSED_ADDR;
	root_inode->ptrs[2].seg_num = LFS_UNUSED_ADDR;
	root_inode->ptrs[2].block_num = LFS_UNUSED_ADDR;
	root_inode->ptrs[3].seg_num = LFS_UNUSED_ADDR;
	root_inode->ptrs[3].block_num = LFS_UNUSED_ADDR;
	memcpy(ifile_buffer + sizeof(struct inode), root_inode, sizeof(struct inode));
	// now ifile_buffer has one inode, which is inode for root directory file. ⬇


	void *root = malloc(sizeof(struct dir_entry) * 3 + sizeof(struct dir));
	// now creating root dir file
	struct dir *root_dir = malloc(sizeof(struct dir));
	strncpy(root_dir->name, "/", sizeof(root_dir->name));
	root_dir->size = 3; 						// ".", ".." and ".ifile", initially 3 "files" in "/"
	void *temp = root;
	memcpy(temp, root_dir, sizeof(struct dir));
	temp = root + sizeof(struct dir);

	struct dir_entry *entry_temp = malloc(sizeof(struct dir_entry));
	strncpy(entry_temp->name, ".", sizeof(entry_temp->name));
	entry_temp->inum = LFS_ROOT_INUM;
	memcpy(temp, entry_temp, sizeof(struct dir_entry));

	temp = root + sizeof(struct dir) + sizeof(struct dir_entry);
	strncpy(entry_temp->name, ".ifile", sizeof(entry_temp->name));
	entry_temp->inum = LFS_IFILE_INUM;
	memcpy(temp, entry_temp, sizeof(struct dir_entry));

	temp = root + sizeof(struct dir) + sizeof(struct dir_entry) * 2;
	strncpy(entry_temp->name, "..", sizeof(entry_temp->name));
	entry_temp->inum = LFS_ROOT_INUM;
	memcpy(temp, entry_temp, sizeof(struct dir_entry));
	// now root is the whole root directory


	// Need to store the initial checkpoint in place. 
	struct checkpoint_region *init_cp = malloc(sizeof(struct checkpoint_region));
	init_cp->timestamp = time(NULL);
	init_cp->last_seg_addr.seg_num = sb_seg_size;	// 也就是1
	init_cp->last_seg_addr.block_num = 0;
	init_cp->next_inum = 2;							// initially the next available inum is 2, after root directory
	init_cp->segment_usage_table = 0;				// TODO dummy
	init_cp->ifile_inode.inum = LFS_IFILE_INUM;
	init_cp->ifile_inode.type = LFS_FILE_TYPE_IFILE;
	init_cp->ifile_inode.size = sizeof(struct inode) * 2;	// TODO need change in phase 2
	init_cp->ifile_inode.ptrs[0].seg_num = sb_seg_size;			// 1
	init_cp->ifile_inode.ptrs[0].block_num = 0;
	init_cp->ifile_inode.ptrs[1].seg_num = LFS_UNUSED_ADDR;			// 1
	init_cp->ifile_inode.ptrs[1].block_num = LFS_UNUSED_ADDR;
	init_cp->ifile_inode.ptrs[2].seg_num = LFS_UNUSED_ADDR;			// 1
	init_cp->ifile_inode.ptrs[2].block_num = LFS_UNUSED_ADDR;
	init_cp->ifile_inode.ptrs[3].seg_num = LFS_UNUSED_ADDR;			// 1
	init_cp->ifile_inode.ptrs[3].block_num = LFS_UNUSED_ADDR;

	// Write ifile, root directory, initial checkpoint region to LFS
	void *total_buffer = malloc(sizeof_block * FLASH_SECTOR_SIZE * 3); 	// 3 blocks spaces. 
	void *buffer_ptr = total_buffer;
	memcpy(buffer_ptr, ifile_buffer, sizeof_block * FLASH_SECTOR_SIZE);
	buffer_ptr = total_buffer + sizeof_block * FLASH_SECTOR_SIZE;		// next block
	memcpy(buffer_ptr, root, sizeof(struct dir_entry) * 3 + sizeof(struct dir));
	buffer_ptr = total_buffer + sizeof_block * FLASH_SECTOR_SIZE * 2;	// next block
	memcpy(buffer_ptr, init_cp, sizeof(struct checkpoint_region));
	Flash_Write(flash, sb_seg_size * sizeof_segment * sizeof_block, 3 * sizeof_block, total_buffer);

	if (Flash_Close(flash) != 0) {
		printf("ERROR: %s\n", strerror(errno));
	}
	return 0;
}
