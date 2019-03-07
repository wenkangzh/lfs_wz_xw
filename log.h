/**
*	@author Wenkang Zhou, Xueheng Wan
*	For CSc 552 LFS Project.
*	
*/

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include "flash.h"
#include <stdlib.h>
#include <string.h>

extern Flash flash;

int Log_Read(uint32_t logAddress, int length, void* buffer);
int Log_Write(int inum, int block, int length, void* buffer, uint32_t* logAddress);
int Log_Free(uint32_t logAddress, int length);

#endif