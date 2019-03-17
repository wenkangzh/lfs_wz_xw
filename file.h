#ifndef FILE_H
#define FILE_H
#define FUSE_USE_VERSION 26

#include "lfs.h"
#include "log.h"


// the file type for this LFS. 1 is normal file, 2 is directory
#define LFS_FILE_TYPE_IFILE 0
#define LFS_FILE_TYPE_FILE 1
#define LFS_FILE_TYPE_DIR 2

struct addr;
struct inode;

int File_Create(int inum, int type);
int File_Write(int inum, int offset, int length, void *buffer);
int File_Write_Helper(int inum, int block, struct addr *blk_addr, int offset, int remaining_length, void *buffer);
int File_Read(int inum, int offset, int length, void *buffer);
int File_Free(int inum);
int File_Read_Helper(struct addr *blk_addr, int offset, int remining_length, void *buffer);
void Read_Inode_in_Ifile(int inum_of_file, struct inode *inode_inum);
int Extend_Inode(int inum, int blk_num);
void Read_Block_in_Ifile(int block_ifile, void *buffer);
#endif