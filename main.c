#include "log.h"
#include "flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char const *argv[])
{
	u_int *blocks = malloc(sizeof(u_int));
	flash = Flash_Open("LFS", FLASH_ASYNC, blocks);
	Log_Write(0,0,0,NULL,NULL);
	void *buffer = malloc(512);
	Log_Read(0, 512, buffer);
	printf("%s\n", (char *)buffer);
	return 0;
}