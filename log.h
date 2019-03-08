/**
*	@author Wenkang Zhou, Xueheng Wan
*	For CSc 552 LFS Project.
*	
*/

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include "flash.h"
#include <stdlib.h>
#include <string.h>
#include "lfs.h"
#include <time.h>

extern Flash flash;

#define SUPERBLOCK_SEG_SIZE 1
#define LFS_SEG(x) x+SUPERBLOCK_SEG_SIZE
#define LFS_SEG_TO_FLASH_SECTOR(lfs_seg_num, lfs_blk_in_seg, lfs_sector_in_blk) lfs_seg_num * lfs_blk_in_seg * lfs_sector_in_blk

int Log_Read(uint32_t logAddress, int length, void* buffer);
int Log_Write(int inum, int block, int length, void* buffer, uint32_t* logAddress);
int Log_Free(uint32_t logAddress, int length);

void get_superblock(struct superblock *sb);

#endif