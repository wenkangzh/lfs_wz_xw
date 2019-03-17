#include "file.h"
#include <stdio.h>


/*
 *----------------------------------------------------------------------
 *
 * File_Create
 *
 *  This function creates a file with the given inum and file type
 * 	A file is represented by an inode containing the metadata for the file, 
 *	including the file type (e.g. file or directory), size, 
 *	and the flash addresses of the file's blocks
 *
 * Parameters:
 *      inum: the inum of the inode of the file to be created. 
 *		type: file type (file or directory).
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int File_Create(int inum, int type)
{
	printf("CREATING file with inum %d\n", inum);

	// if the file is actually a directory, then the content of the "special file" is array of <name, inum> pairs
	// i would guess that the format of the special file is up to us/designers
	// since the inodes are stored in ifile, whose inode is in the most recent checkpoint, 

	// we need to change(or actually append) the ifile

	// locate ifile and get the inode of inum



	// extend ifile if inum is in a unassigned block of ifile
	int blk_in_ifile = inum * sizeof(struct inode) / s_block_byte;
	if(cp_region->ifile_inode.ptrs[blk_in_ifile].seg_num == LFS_UNUSED_ADDR) {
		if(Extend_Inode(LFS_IFILE_INUM, blk_in_ifile) == -1){
			printf("File Write Operation Termination: Number of File Limitation Reached!)\n");
			return -1;
		}
	}
	// Find inode of file
	struct inode *inode_inum = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode_inum);
	// Implement inode of inum
	inode_inum->inum = inum;
	inode_inum->type = type;
	inode_inum->size = 0;
	inode_inum->ptrs[0].seg_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[1].seg_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[2].seg_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[3].seg_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[0].block_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[1].block_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[2].block_num = LFS_UNUSED_ADDR;
	inode_inum->ptrs[3].block_num = LFS_UNUSED_ADDR;
	// Write the modified block of ifile through Log_Write
	void *ifile_blk = malloc(s_block_byte);
	Read_Block_in_Ifile(blk_in_ifile ,ifile_blk);
	void *modificaition = ifile_blk + inum * sizeof(struct inode) % s_block_byte;
	memcpy(modificaition, inode_inum, sizeof(struct inode));
	struct addr *logAddress = malloc(sizeof(struct addr));
	int i = Log_Write(LFS_IFILE_INUM, blk_in_ifile, s_block_byte, ifile_blk, logAddress);
	if (i != 0){
		return i;
	}
	// Update inode of ifile
	cp_region->ifile_inode.ptrs[blk_in_ifile].seg_num = logAddress->seg_num;
	cp_region->ifile_inode.ptrs[blk_in_ifile].block_num = logAddress->block_num;
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * File_Write
 *
 *  This function writes a file with the inum of the inode, given the offset, length and write what's in buffer
 *
 * Parameters:
 *      inum: the inum of the inode of the file to be written. 
 *		offset: the starting offset of the writing in bytes. 
 *		length: the length of the writing in bytes.
 *		buffer: the data need to be written into the file. 
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int File_Write(int inum, int offset, int length, void *buffer)
{
	// Locate inode of inum in ifile
	struct inode *inode_inum = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode_inum);
	// write block of the file
	int data_written = 0;
	int remaining_length = length - data_written;
	int i_blk = offset / s_block_byte; //Write starts at 'i_blk'th block
	while(remaining_length != 0){
		printf("....%u\n", inode_inum->ptrs[i_blk].seg_num);
		if(inode_inum->ptrs[i_blk].seg_num == LFS_UNUSED_ADDR) {
			
			if(Extend_Inode(inum, i_blk) == -1){
				printf("File Write Operation Termination: Number of File Limitation Reached!)\n");
				return -1;
			}
			Read_Inode_in_Ifile(inum, inode_inum);
			print_inode(inode_inum);
		}
		data_written += File_Write_Helper(	inum, 
											i_blk, 
											&(inode_inum->ptrs[i_blk]),
											(offset + data_written) % s_block_byte, 
											remaining_length, 
											buffer + data_written	);
		remaining_length = length - data_written;
		i_blk ++;
		// create a new block for appened data
	}
	return 0;
}

// Read data from given offset to the end of the block
int File_Write_Helper(int inum, int block, struct addr *blk_addr, int offset, int remaining_length, void *buffer){
	// Get target block that being replaced
	void *block_data = malloc(s_block_byte);
	File_Read(inum, inum * s_block_byte, s_block_byte, block_data);
	// Modify the target block
	int write_length = s_block_byte - offset; // Calculate how many bytes we need read
	if(write_length > remaining_length) write_length = remaining_length;
	void* modificaition = block_data + offset;
	memcpy(modificaition, buffer, write_length);
	// Write new data of block
	Log_Write(inum, block, write_length, block_data, blk_addr);
	return write_length;
}

/*
 *----------------------------------------------------------------------
 *
 * File_Read
 *
 *  This function read from a file with inum of the inode, given the offset, length and the destination of reading as buffer
 *
 * Parameters:
 *      inum: the inum of the inode of the file to be read. 
 *		offset: the starting offset of the reading in bytes. 
 *		length: the length of the reading in bytes.
 *		buffer: the data need to be read from the file. 
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int File_Read(int inum, int offset, int length, void *buffer)
{
	// For reading a file, the process may be:
	// 1. Find the ifile. Note: inode of ifile is in the checkpoint. (Thus we could keep the inode of the ifile in memory.)
	// 2. Using the given inum, locate the inode(inside the ifile) of the wanted file
	// 3. In the inode, there are four direct block pointer and (TODO in phase 2, indirect block)
	// 4. Calculate which and how many block need to be read, get them using Log_Read()
	// 5. memcpy() the wanted data starting at offset and length bytes into buffer


	// locate ifile and get the inode of inum
	// void  *ifile_blk = malloc(lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// int blk_in_ifile = (inum * sizeof(struct inode)) / (FLASH_SECTOR_SIZE * lfs_sb->b_size);
	// struct addr *ifile_addr = malloc(sizeof(struct addr));
	// ifile_addr->seg_num = cp_region->ifile_inode.ptrs[blk_in_ifile].seg_num;
	// ifile_addr->block_num = (inum * sizeof(struct inode)) % (FLASH_SECTOR_SIZE * lfs_sb->b_size);
	// Log_Read(ifile_addr, FLASH_SECTOR_SIZE * lfs_sb->b_size, ifile_blk);
	// struct inode *inode_inum = ifile_blk + (inum * sizeof(struct inode)) % (FLASH_SECTOR_SIZE * lfs_sb->b_size);
	struct inode *inode_inum = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode_inum);

	print_inode(inode_inum);

	// read block of the file
	int data_read = 0;
	int remaining_length = length - data_read;
	int i_blk = offset / s_block_byte;
	while(remaining_length != 0){
		data_read += File_Read_Helper(	&(inode_inum->ptrs[i_blk]), 
										(offset + data_read) % s_block_byte, 
										remaining_length, 
										buffer + data_read	);
		remaining_length = length - data_read;
		i_blk ++;
	}
	return 0;
}

// Read data from given offset to the end of the block
int File_Read_Helper(struct addr *blk_addr, int offset, int remaining_length, void *buffer){
	int read_length = s_block_byte - offset; // Calculate how many bytes we need read
	if(read_length > remaining_length) read_length = remaining_length;
	Log_Read(blk_addr, read_length, buffer);
	return read_length;
}

/*
 *----------------------------------------------------------------------
 *
 * File_Free
 *
 *  This function frees the file with the inode with the given inum.
 *
 * Parameters:
 *      inum: 
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int File_Free(int inum)
{
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Read_Ifile
 *
 *  This function reads ifile, return a block of ifile.
 *
 * Parameters:
 *      
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
void Read_Inode_in_Ifile(int inum_of_file, struct inode *inode_inum){
	void  *ifile_blk = malloc(s_block_byte);
	int blk_in_ifile = (inum_of_file * sizeof(struct inode)) / s_block_byte;
	struct addr *ifile_addr = malloc(sizeof(struct addr));
	ifile_addr->seg_num = cp_region->ifile_inode.ptrs[blk_in_ifile].seg_num;
	ifile_addr->block_num = cp_region->ifile_inode.ptrs[blk_in_ifile].block_num;
	printf("ifile block %u %u\n", ifile_addr->seg_num, ifile_addr->block_num);
	Log_Read(ifile_addr, s_block_byte, ifile_blk);
	memcpy(inode_inum, ifile_blk + (inum_of_file * sizeof(struct inode)) % s_block_byte, sizeof(struct inode));
}

void Read_Block_in_Ifile(int block_ifile, void *buffer){
	struct addr *block_addr = malloc(sizeof(struct addr));
	block_addr->seg_num = cp_region->ifile_inode.ptrs[block_ifile].seg_num;
	block_addr->block_num = cp_region->ifile_inode.ptrs[block_ifile].block_num;
	printf("ifile block %u %u\n", block_addr->seg_num, block_addr->block_num);
	Log_Read(block_addr, s_block_byte, buffer);
}

// Create a block for ifile
int Extend_Inode(int inum, int blk_num){
	if(blk_num > 3){
		printf("ifile extension: Limitation of ifile is reached! (Current ifile has 4 blocks)\n");
		return -1;
	}
	void *new_block = malloc(s_block_byte);
	struct addr *new_block_address = malloc(sizeof(struct addr));
	Log_Write(inum, blk_num, s_block_byte, new_block, new_block_address);
	printf(">>> %u %u ", new_block_address->seg_num, new_block_address->block_num);
	return 0;
}