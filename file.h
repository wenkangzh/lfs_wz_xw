#ifndef FILE_H
#define FILE_H

// the file type for this LFS. 1 is normal file, 2 is directory
#define LFS_FILE_TYPE_IFILE 0
#define LFS_FILE_TYPE_FILE 1
#define LFS_FILE_TYPE_DIR 2

int File_Create(int inum, int type);
int File_Write(int inum, int offset, int length, void *buffer);
int File_Read(int inum, int offset, int length, void *buffer);
int File_Free(int inum);



#endif