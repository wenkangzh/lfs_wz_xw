#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "flash.h"
#include <errno.h>
#include "lfs.h"
#include "log.h"

int main(int argc, char *argv[])
{
	int sizeof_block = 2; 		// size of a block, in sectors
	int sizeof_segment = 32;  	// segment size, in blocks, must be multiple of flash erase block size
	int sizeof_flash = 100; 	// size of flash, in segments.
	int wearlimit = 1000;		// wear limit for erase blocks
	char *filename = argv[argc-1];

	int opt = 0;
	struct option long_options[] = {
		{"block", 		required_argument, 0, 'b'}, 
		{"segment", 	required_argument, 0, 'l'},
		{"segments", 	required_argument, 0, 's'},
		{"wearlimit", 	required_argument, 0, 'w'}
	};
	int long_index = 0;
	while ((opt = getopt_long(argc, argv, "b:l:s:w:", 
					long_options, &long_index)) != -1){
		switch (opt) {
			case 'b':
				sizeof_block = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'l':
				sizeof_segment = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 's':
				sizeof_flash = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'w':
				wearlimit = (int) strtol(optarg, (char **)NULL, 10);
				break;
			default:
				printf("usage: ... \n");
		}
	}

	printf("size of block: %d sectors\n", sizeof_block);
	printf("size of segment: %d blocks\n", sizeof_segment);
	printf("size of flash: %d segments\n", sizeof_flash);
	printf("wear limit: %d blocks\n", wearlimit);
	printf("Filename: %s\n", filename);

	int sizeof_flash_in_blocks = sizeof_flash * sizeof_segment;
	if(Flash_Create(filename, wearlimit, sizeof_flash_in_blocks) != 0){
		printf("ERROR: %s\n", strerror(errno));
	}
	// create done. 
	// now do initialization of the LFS
	/*struct superblock *superblock = malloc(sizeof(struct superblock));
	superblock->seg_size = sizeof_segment;
	superblock->b_size = sizeof_block;
	superblock->seg_num = sizeof_flash;
	superblock->sb_seg_num = SUPERBLOCK_SEG_SIZE;
	superblock->checkpoint_addr.seg_num = LFS_SEG(0);
	superblock->checkpoint_addr.block_num = 0;

	struct checkpoint_region *cp0 = malloc(sizeof(struct checkpoint_region));
	cp0->timestamp = time(NULL);
	cp0->last_seg_addr.seg_num = LFS_SEG(0);
	cp0->last_seg_addr.block_num = 0;
	cp0->last_seg_addr.ifile_inode = malloc(sizeof(struct inode));
`	*/


	u_int *n_blocks = malloc(sizeof(u_int));
	Flash flash = Flash_Open(filename, FLASH_ASYNC, n_blocks);
	printf("%u\n", *n_blocks);
	Flash_Write(flash, 0, sizeof_block * sizeof_segment * SUPERBLOCK_SEG_SIZE, superblock);
	


	return 0;
}