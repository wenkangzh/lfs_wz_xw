/**
*	@author Wenkang Zhou, Xueheng Wan
*	For CSc 552 LFS Project.
*	
*/
#define FUSE_USE_VERSION 26
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "flash.h"
#include "lfs.h"

extern Flash flash;
extern struct segment *tail_seg;

#define SUPERBLOCK_SEG_SIZE 1
#define LFS_SEG(x) x+SUPERBLOCK_SEG_SIZE
// #define LFS_SEG_TO_FLASH_SECTOR(lfs_seg_num, lfs_blk_in_seg, lfs_sector_in_blk) lfs_seg_num * lfs_blk_in_seg * lfs_sector_in_blk
// #define LFS_ADDR_TO_FLASH_SECTOR(lfs_seg_num, lfs_blk_num, lfs_blk_in_seg, lfs_sector_in_blk) lfs_seg_num * lfs_blk_in_seg * lfs_sector_in_blk + lfs_blk_num * lfs_sector_in_blk

#define BLOCK_UNUSED 0
#define BLOCK_DATA 1
#define BLOCK_IFILE 2
struct addr;

struct segment{
	uint16_t seg_num;				// the segment number of this segment
	void *blocks;					// blocks in 1 segment
};

int Log_Read(struct addr *logAddress, int length, void* buffer);
int Log_Write(int inum, int block, int length, void* buffer, struct addr *logAddress);
int Log_Free(uint32_t logAddress, int length);
void update_sb();
int logAddr_To_Sectors(struct addr *addr);
int segNum_To_Sectors(uint16_t seg_num);
#endif