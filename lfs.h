/**
*	@author Wenkang Zhou, Xueheng Wan
*	For CSc 552 LFS Project.
*	
*/
#ifndef LFS_H
#define LFS_H

#include <stdio.h>
#include <stdint.h>
#include "flash.h"
#include <stdlib.h>
#include <string.h>

struct addr{
	uint16_t seg_num;						// segment number
	uint16_t block_num;						// block number within the segment
};

struct checkpoint_region{
	time_t timestamp;						// timestamp, time_t is uint64_t
	struct addr last_seg_addr;				// last segment written
	struct addr inode_map_addr;				// address of inode map
	uint32_t segment_usage_table;			// PLACEHOLDER for segment usage table. 
};

struct superblock {
	uint16_t seg_size; 						// segment size, in blocks
	uint16_t b_size;						// block size, in sectors
	uint16_t seg_num;						// # of segments
	uint16_t sb_seg_num;					// # of segments for superblock usage
	struct checkpoint_region ck_region_0;	// checkpoint region #0
	struct checkpoint_region ck_region_1;	// checkpoint region #1
};

#endif