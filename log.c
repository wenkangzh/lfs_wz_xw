#include "log.h"

Flash flash = NULL;

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
int Log_Read(uint32_t logAddress, int length, void* buffer)
{
	Flash_Read(flash, 0, 1, buffer);
	return 0;
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
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Log_Write(int inum, int block, int length, void* buffer, uint32_t* logAddress)
{
	if(flash == NULL){
		printf("Flash is not initialized!\n");
	}
	struct superblock *sb = malloc(sizeof(struct superblock));
	get_superblock(sb);

	// Find the segment to be written
	struct addr *insert_ptr = malloc(sizeof(struct addr));
	printf("%ld %ld\n", sb->ck_region_0.timestamp, sb->ck_region_1.timestamp);
	if(sb->ck_region_0.timestamp == 0 && sb->ck_region_1.timestamp == 0){
		insert_ptr->seg_num =LFS_SEG(0);
		insert_ptr->block_num = 0;
		printf("A");
	}else if(sb->ck_region_0.timestamp == 0){
		insert_ptr = &(sb->ck_region_1.last_seg_addr);
		printf("B");
	}else{
		insert_ptr = difftime(sb->ck_region_0.timestamp, sb->ck_region_1.timestamp) < 0 ? &(sb->ck_region_1.last_seg_addr) : &(sb->ck_region_0.last_seg_addr);
		printf("C");
	}

	insert_ptr->seg_num = (insert_ptr->seg_num) % (sb->seg_size - 1);

	// Write to Flash
	printf("sector offset: %d %d %d\n", insert_ptr->seg_num, sb->seg_size, sb->b_size);
	printf("sector count: %d\n", length / FLASH_SECTOR_SIZE + (length % FLASH_SECTOR_SIZE == 0 ? 0 : 1));
	return Flash_Write(flash, LFS_SEG_TO_FLASH_SECTOR(insert_ptr->seg_num, sb->seg_size, sb->b_size), length / FLASH_SECTOR_SIZE + (length % FLASH_SECTOR_SIZE == 0 ? 0 : 1), buffer);
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



void get_superblock(struct superblock *sb)
{
	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	Flash_Read(flash, 0, 64, buffer);
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer + sizeof(uint16_t));
	int sectors = *seg_size * *b_size;
	void *temp = malloc(sectors * FLASH_SECTOR_SIZE);
	// printf("%d\n", sectors);
	Flash_Read(flash, 0, sectors, temp);
	memcpy(sb, temp, sizeof(struct superblock));

}
