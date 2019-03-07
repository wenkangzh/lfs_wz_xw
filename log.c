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
 *      inum:
 *		block: 
 *		length: 
 *		buffer: 
 *		&logAddress:
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
	void *b = malloc(8);
	memset(b, 'A', 8);
	Flash_Write(flash,0,1,b);
	Flash_Write(flash,1,1,b);
	return 0;
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