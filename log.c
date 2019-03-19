#include "log.h"

Flash flash = NULL;
struct segment *tail_seg = NULL;
int next_block_in_tail = 0;

/*
 *----------------------------------------------------------------------
 *
 * Log_Read
 *
 *  This function read a block given a logAddress structure with length.
 *
 * Parameters:
 *      logAddress: The given "address" in struct addr structure. 
 *		length: the length to be read
 * 		buffer: the destination of the operation.
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Log_Read(struct addr *logAddress, int length, void* buffer)
{
	printf("READING from %u %u\n", logAddress->seg_num, logAddress->block_num);
	// if the block we need to find is in the "tail" segment, just read from the tail cache.
	if(logAddress->seg_num == tail_seg->seg_num){
		printf("READING FROM TAIL CACHE, SEG_NUM: %u\n", tail_seg->seg_num);
		void *block_to_copy = tail_seg->blocks + (lfs_sb->b_size * FLASH_SECTOR_SIZE) * (logAddress->block_num);
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


// update the block in the ifile, return the block
void updateInode(int inum, int block, struct addr *block_addr, int length)
{
	printf("UPDATING INODE OF FILE %d WITH SEG_NUM: %u AND BLOCK_NUM: %u\n", inum, block_addr->seg_num, block_addr->block_num);
	// 1. Find the block in ifile where the inode locates.
	void *buffer = malloc(lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// find the block number of ifile:
	int blk_num = inum * sizeof(struct inode) / (lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// read the necessary file block from ifile for reading the inode given the inum
	if(Log_Read(&cp_region->ifile_inode.ptrs[blk_num], lfs_sb->b_size * FLASH_SECTOR_SIZE, buffer) == -1){ // get data of the ifile block
			printf("updateInode: Failed to read target block of iFile.\n");
	}
	// 2. Find inode with inum in ifile.
	int offset = inum * sizeof(struct inode) % s_block_byte;
	struct inode *i_inode = (struct inode *) (buffer + offset);
	// 3. Update the address of the file block "block"
	i_inode->ptrs[block].seg_num = block_addr->seg_num;
	i_inode->ptrs[block].block_num = block_addr->block_num;
	i_inode->size += length; // length > 0 if the size increase; length < 0 if the size decrease;
	// writing the file block of ifile in the log.
	//print_inode(i_inode);
	Log_Write(LFS_IFILE_INUM, blk_num, s_block_byte, buffer, &(cp_region->ifile_inode.ptrs[blk_num]));
	cp_region->ifile_inode.size = cp_region->ifile_inode.size < (inum + 1) * sizeof(struct inode) ? (inum + 1) * sizeof(struct inode) : cp_region->ifile_inode.size;
	printf("iFile has been updated with FILE %d :\n", inum);
	print_inode(&(cp_region->ifile_inode));
}


int write_tail_seg_to_flash()
{
	int i;
	printf("WRITING TO FLASH. %d %u\n", segNum_To_Sectors(tail_seg->seg_num), lfs_sb->seg_size * lfs_sb->b_size);
	i = Flash_Write(flash, segNum_To_Sectors(tail_seg->seg_num), lfs_sb->seg_size * lfs_sb->b_size, tail_seg->blocks);
	if(i != 0){
		printf("ERROR: %s\n", strerror(errno));
		return i;
	}

	tail_seg->seg_num++; // TODO get next segment number from most recent checkpoint region. 
	memset(tail_seg->blocks, 0, lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	next_block_in_tail = 0;

	// TODO add this segment to segment cache. 
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Write
 *
 *  This function write a block in the log
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
int Log_Write(int inum, int block, int length, void* buffer, struct addr *logAddress)
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
	void *next_block = tail_seg->blocks + s_block_byte * next_block_in_tail;
	// printf("HEAD: %p NEXT: %p, skipped %u\n", tail_seg->blocks, next_block, (lfs_sb->b_size * FLASH_SECTOR_SIZE) * next_block_in_tail);
	memcpy(next_block, buffer, s_block_byte);

	// if Log_Write is used to write iFile, *logAddress is acutally the cp_region->ifile_inode.ptrs[i];
	(logAddress)->seg_num = tail_seg->seg_num;
	(logAddress)->block_num = next_block_in_tail++;

	int i = 0;
	// check if tail_seg is full, TODO RESERVE 1 block for checkpoint region.
	if (next_block_in_tail == lfs_sb->seg_size - 1){
		i = write_tail_seg_to_flash();
	}

	// update the inode for the file
	if(inum != LFS_IFILE_INUM && inum != -1)
		updateInode(inum, block, (logAddress), length);

	printf("ADRESS OF NEW WRITTEN BLOCK IN LOG: %u %u \n", logAddress->seg_num, logAddress->block_num);
	return i;
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Free
 *
 *  This function frees a block with given logAddress
 *
 * Parameters:
 *      logAddress: the address indicates the block to be free.
 *		length: 	the number of bytes to free
 *  
 * Return: 0 - success, 1 - failure
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

