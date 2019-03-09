#include "log.h"

Flash flash = NULL;
struct segment *tail_seg = NULL;
int next_block_in_tail = 0;

/*
 *----------------------------------------------------------------------
 *
 * Log_Read
 *
 *  This function 
 *
 * Parameters:
 *      logAddress: 
 *		length: 
 * 		buffer: 
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Log_Read(struct addr *logAddress, int length, void* buffer)
{
	// if the block we need to find is in the "tail" segment, just read from the tail cache.
	if(logAddress->seg_num == tail_seg->seg_num){
		void *block_to_copy = tail_seg->blocks + (lfs_sb->b_size * FLASH_SECTOR_SIZE) * (logAddress->block_num-1);
		memcpy(buffer, block_to_copy, (lfs_sb->b_size * FLASH_SECTOR_SIZE));
		return 0;
	}

	// TODO if the block we need to find is in the segment cache, just read from the cache. 
	if(1){

	}

	u_int sector_offset = logAddr_To_Sectors(logAddress);
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = length / FLASH_SECTOR_SIZE + (length % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp = malloc(FLASH_SECTOR_SIZE * sector_n);
	int i = Flash_Read(flash, sector_offset, sector_n, temp);
	if(i == 1){
		printf("ERROR: %s\n", strerror(errno));
		return i;
	}
	memcpy(buffer, temp, length);
	// when returning the buffer as a read result, need to fix the size of buffer to be length
	return i;
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Write
 *
 *  This function 
 *
 * Parameters:
 *      inum:			inode number of the file
 *		block: 			block number within the file (block size is 1024 bytes by default)
 *		length: 		number of bytes to write
 *		buffer: 		the data to write
 *		&logAddress:	indicate the log segment and block number within the segment
 *  
 * Return: -1 error
 *
 *----------------------------------------------------------------------
 */
int Log_Write(int inum, int block, int length, void* buffer, struct addr **logAddress)
{
	if(flash == NULL){
		printf("LOG_WRITE: Flash is not initialized!\n");
		return -1;
	}

	// length validation for maximum size of block, 1024 bytes by default
	if(length > lfs_sb->b_size * FLASH_SECTOR_SIZE){
		printf("LOG_WRITE: length too large.\n");
		return -1;
	}

	// insert into tail_seg
	void *next_block = tail_seg->blocks + (lfs_sb->b_size * FLASH_SECTOR_SIZE) * next_block_in_tail;
	memcpy(next_block, buffer, length);

	(*logAddress)->seg_num = tail_seg->seg_num;
	(*logAddress)->block_num = next_block_in_tail++;

	int i;
	// check if tail_seg is full, TODO RESERVE 1 block for checkpoint region.
	if (next_block_in_tail == lfs_sb->seg_size - 2){
		i = Flash_Write(flash, segNum_To_Sectors(tail_seg->seg_num), lfs_sb->seg_size * lfs_sb->b_size, buffer);
		if(i != 0){
			printf("ERROR: %s\n", strerror(errno));
			return i;
		}
		tail_seg->seg_num++; // TODO get next segment number from most recent checkpoint region. 
		memset(tail_seg->blocks, 0, lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
		next_block_in_tail = 0;

		// TODO add this segment to segment cache. 
	}

	return i;
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Free
 *
 *  This function 
 *
 * Parameters:
 *      logAddress: 
 *		length:
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Log_Free(uint32_t logAddress, int length)
{
	return 0;
}

void update_sb()
{
	Flash_Write(flash, 0, lfs_sb->seg_size * lfs_sb->b_size * lfs_sb->seg_num, lfs_sb);
}

int logAddr_To_Sectors(struct addr *addr)
{
	return addr->seg_num * lfs_sb->seg_size * lfs_sb->b_size + addr->block_num * lfs_sb->b_size;
}

int segNum_To_Sectors(uint16_t seg_num)
{
	return seg_num * lfs_sb->seg_size * lfs_sb->b_size;
}

