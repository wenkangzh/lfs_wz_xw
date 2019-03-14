#ifndef FILE_H
#define FILE_H


int File_Create(int inum, int type);
int File_Write(int inum, int offset, int length, void *buffer);
int File_Read(int inum, int offset, int length, void *buffer);
int File_Free(int inum);



#endif