/**
*	@author Wenkang Zhou, Xueheng Wan
*	For CSc 552 LFS Project.
*	
*/
#define FUSE_USE_VERSION 26
#ifndef LFS_H
#define LFS_H

#include <stdio.h>
#include <stdint.h>
#include "flash.h"
#include <stdlib.h>
/**
*	filename: lfs.h
*	CSC 552 LFS Project Phase 1
*	Wenkang Zhou wenkangzhou@email.arizona.edu
*	Xueheng Wan  wanxueheng@email.arizona.edu
*/

#include <string.h>
#include "file.h"
#include <fuse.h>
#include <string.h>
#include <fcntl.h>

extern struct superblock *lfs_sb;
extern struct checkpoint_region *cp_region;
extern int s_block_byte;

#define LFS_IFILE_INUM 0
#define LFS_ROOT_INUM 1

#define LFS_UNUSED_ADDR 0xFFFF

#define LFS_FILE_NAME_MAX_LEN 48

struct addr{
	uint16_t seg_num;						// segment number
	uint16_t block_num;						// block number within the segment
};

struct inode{
	uint16_t inum;							// inum
	uint16_t type;							// type of the file, file or directory
	mode_t mode;							// mode of the file
	time_t lst_mdf;							// last modification time
	uint16_t n_links;						// # of hard links
	int size;								// size of the file.
	struct addr ptrs[4];
	char padding[18];
	// TODO In phase 2, implement a indirect pointer to a block of direct pointers
};

struct checkpoint_region{
	time_t timestamp;						// timestamp, time_t is uint64_t
	struct addr last_seg_addr;				// last segment written
	uint32_t next_inum;						// the next available inum in the system. 
	uint32_t segment_usage_table;			// PLACEHOLDER for segment usage table. 
	struct inode ifile_inode;				// inode of THE ifile
};

struct superblock {
	uint16_t seg_size; 						// segment size, in blocks
	uint16_t b_size;						// block size, in sectors
	uint16_t seg_num;						// # of segments
	uint16_t sb_seg_num;					// # of segments for superblock usage
	struct addr checkpoint_addr;			// address of the current(most recent) checkpoint region
};

struct dir {
	char name[LFS_FILE_NAME_MAX_LEN];
	int size;								// the size of entries
	// here will be dir_entry, number of "size"
};

struct dir_entry{
	char name[LFS_FILE_NAME_MAX_LEN];
	uint16_t inum;
};

void init_sb();
void print_sb();
void print_cp_region();
void print_inode(struct inode *inode);
uint16_t find_last_dir(const char* path, char *new_dir);
uint16_t inum_lookup(const char *path);
uint16_t inum_lookup_from_parent_inum(char *filename, uint16_t parent_inum);
void substring(char s[], char sub[], int p, int l);
void update_cp_region();

#endif