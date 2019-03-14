#include "file.h"


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

}