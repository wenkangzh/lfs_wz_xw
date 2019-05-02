/**
*	filename: log.h
*	CSC 552 LFS Project Phase 1
*	Wenkang Zhou wenkangzhou@email.arizona.edu
*	Xueheng Wan  wanxueheng@email.arizona.edu
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
extern int max_size_seg_cache;
extern int periodic_cp_interval;
extern int size_seg_summary;
extern int SUPERBLOCK_SEG_SIZE;
extern int cleaner_pointer;
extern char *flash_file_name;

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

struct LinkedList{
	void *segment;
	uint16_t seg_num;
	struct LinkedList * next;
	struct LinkedList * prev;
};

int Log_Read(struct addr *logAddress, int length, void* buffer);
int write_tail_seg_to_flash();
int Log_Write(int inum, int block, int length, void* buffer, struct addr *logAddress);
void updateInode(int inum, int block, struct addr *block_addr, int length);
void getAddr(int inum, int i, struct addr *address);
void createIndirectBlock(int inum, struct addr *address);
void cleaner_start();
void cleaner(int seg_num);
int needClean(int seg_num);
int isBlockAlive(int inum, struct addr *seg_addr);
int isTwoAddressSame(struct addr *addr1, struct addr *addr2);
void findNextAvailbleSegment();
int Log_Free(struct addr *logAddress, int length);
void init_seg_summary();
void set_seg_summary(int block_num, uint16_t inum);
uint16_t get_block_inum(void* segment, int index);
void load_segment_usage_table();
void update_segment_usage_table(int seg_num, int isUnvailble);
void write_segment_usage_table();
void update_sb();
int logAddr_To_Sectors(struct addr *addr);
int segNum_To_Sectors(uint16_t seg_num);
int LFS_SEG(int x);
int Seg_Cache_init(int N);
void SC_trim();
int SC_push();
int SC_get(uint16_t segment_num, void *buffer);
void SC_print();
#endif
