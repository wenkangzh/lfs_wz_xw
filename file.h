/**
*	filename: file.h
*	CSC 552 LFS Project Phase 1
*	Wenkang Zhou wenkangzhou@email.arizona.edu
*	Xueheng Wan  wanxueheng@email.arizona.edu
*/

#ifndef FILE_H
#define FILE_H
#define FUSE_USE_VERSION 26

#include "lfs.h"
#include "log.h"


// the file type for this LFS. 1 is normal file, 2 is directory
#define LFS_FILE_TYPE_IFILE 0
#define LFS_FILE_TYPE_FILE 1
#define LFS_FILE_TYPE_DIR 2
#define LFS_FILE_TYPE_SYMLINK 3

extern struct superblock *lfs_sb;
extern struct checkpoint_region *cp_region;
extern int s_block_byte;

struct addr;
struct inode;

int File_Create(int inum, int type);
int File_Write(int inum, int offset, int length, void *buffer);
int File_Write_Helper(struct inode *inode_inum, int block, int offset, int remaining_length, void *buffer, int isNewBlock);
int File_Read(int inum, int offset, int length, void *buffer);
int File_Free(int inum);
int File_Read_Helper(struct addr *blk_addr, int offset, int remining_length, void *buffer);
void Read_Inode_in_Ifile(int inum_of_file, struct inode *inode_inum);
int Extend_Inode(int inum, int blk_num);
void write_inode_in_ifile(int inum_of_file, struct inode *inode_inum);
void Read_Block_in_Ifile(int block_ifile, void *buffer);
#endif
