/**
 *	filename: log.c
 *	CSC 552 LFS Project Phase 1
 *	Wenkang Zhou wenkangzhou@email.arizona.edu
 *	Xueheng Wan  wanxueheng@email.arizona.edu
 */

#include "log.h"

Flash flash = NULL;
struct segment *tail_seg = NULL;
int next_block_in_tail;

struct LinkedList *head;
struct LinkedList *tail;

int size_of_seg_cache = 0;
int write_counter = 0;

void *segUsageTable;
int free_segment_counter;
/*
 *----------------------------------------------------------------------
 *
 * Log_Read
 *
 *  This function read a block given a logAddress structure with length.
 *
 * Parameters:
 *      logAddress: The given "address" in struct addr structure. 
 *		length: the length to be read
 * 		buffer: the destination of the operation.
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Log_Read(struct addr *logAddress, int length, void* buffer) {
	printf("READING from %u %u\n", logAddress->seg_num, logAddress->block_num);
	SC_print();
	// if the block we need to find is in the "tail" segment, just read from the tail cache.
	if (logAddress->seg_num == tail_seg->seg_num) {
		void *block_to_copy = tail_seg->blocks
				+ (lfs_sb->b_size * FLASH_SECTOR_SIZE)
						* (logAddress->block_num);
		printf("READING SEGMENT FROM {TAIL CACHE}, SEG_NUM: %u\n",
				tail_seg->seg_num);
		memcpy(buffer, block_to_copy, (lfs_sb->b_size * FLASH_SECTOR_SIZE));
		return 0;
	}

	//TODO if the block we need to find is in the segment cache, just read from the cache. 
	void *buffer1 = malloc(
			lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	if (SC_get(logAddress->seg_num, buffer1) == 0) {
		void *block_to_copy = buffer1
				+ (lfs_sb->b_size * FLASH_SECTOR_SIZE)
						* (logAddress->block_num);
		memcpy(buffer, block_to_copy, (lfs_sb->b_size * FLASH_SECTOR_SIZE));
		printf(
				">>>>>>>READING SEGMENT FROM SEGMENT {SEGMENT CACHE}, SEG_NUM: %u %s\n",
				logAddress->seg_num, (char*) buffer);
		return 0;
	}
	free(buffer1);

	u_int sector_offset = logAddr_To_Sectors(logAddress);
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = length / FLASH_SECTOR_SIZE
			+ (length % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp = malloc(FLASH_SECTOR_SIZE * sector_n);
	int i = Flash_Read(flash, sector_offset, sector_n, temp);
	if (i == 1) {
		printf("ERROR: %s\n", strerror(errno));
		return i;
	}
	memcpy(buffer, temp, length);
	// when returning the buffer as a read result, need to fix the size of buffer to be length
	return i;
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Write
 *
 *  This function write a block in the log
 *
 * Parameters:
 *      inum:			inode number of the file
 *		block: 			block number within the file (block size is 1024 bytes by default)
 *		length: 		number of bytes to write
 *		buffer: 		the data to write
 *		&logAddress:	indicate the log segment and block number within the segment
 *  
 * Return: -1 error
 *
 *----------------------------------------------------------------------
 */
int Log_Write(int inum, int block, int length, void* buffer,
		struct addr *logAddress) {
	if (flash == NULL) {
		printf("LOG_WRITE: Flash is not initialized!\n");
		return -1;
	}

	// length validation for maximum size of block, 1024 bytes by default
	if (length > lfs_sb->b_size * FLASH_SECTOR_SIZE) {
		printf("LOG_WRITE: length too large.\n");
		return -1;
	}

	// Clean Mechanism
	cleaner_start();

	// insert into tail_seg
	void *next_block = tail_seg->blocks + s_block_byte * next_block_in_tail;
	// printf("HEAD: %p NEXT: %p, skipped %u\n", tail_seg->blocks, next_block, (lfs_sb->b_size * FLASH_SECTOR_SIZE) * next_block_in_tail);
	memcpy(next_block, buffer, s_block_byte);

	// if Log_Write is used to write iFile, *logAddress is acutally the cp_region->ifile_inode.ptrs[i];
	(logAddress)->seg_num = tail_seg->seg_num;
	(logAddress)->block_num = next_block_in_tail++;

	// Update Segment Summary Table
	set_seg_summary(next_block_in_tail - 1, inum);

	int i = 0;
	// check if tail_seg is full, TODO RESERVE 1 block for checkpoint region.
	if (next_block_in_tail == lfs_sb->seg_size) {
		i = write_tail_seg_to_flash();
	}

	// update the inode for the file
	if (inum != LFS_IFILE_INUM && inum != -1 && block >= 0)
		updateInode(inum, block, (logAddress), length);

	printf("ADRESS OF NEW WRITTEN BLOCK IN LOG: %u %u \n", logAddress->seg_num,
			logAddress->block_num);

	SC_print();
	return i;
}

int write_tail_seg_to_flash() {
	int i;
	printf("WRITING TO FLASH. %d %u\n", segNum_To_Sectors(tail_seg->seg_num),
			lfs_sb->seg_size * lfs_sb->b_size);
	i = Flash_Write(flash, segNum_To_Sectors(tail_seg->seg_num),
			lfs_sb->seg_size * lfs_sb->b_size, tail_seg->blocks);
	if (i != 0) {
		printf("ERROR: %s\n", strerror(errno));
		return i;
	}

	// Update Checkpoint region
	write_counter++;
	if (write_counter % periodic_cp_interval == 0) {
		update_cp_region();
		write_counter = 0;
	}

	// Update segment usage table
	update_segment_usage_table(tail_seg->seg_num, 1);

	// Update free segment counts
	free_segment_counter--;

	// Update segment cache
	if (SC_push() == -1)
		printf("FAILED TO INSERT THIS SEGMENT INTO SEGMENT CACHE!\n");
	else
		printf(
				"***************THIS SEGMENT HAS BEEN INSERTED IN SEGMENT CACHE!****************\n");

	findNextAvailbleSegment(); // TODO get next segment number from most recent checkpoint region.
	memset(tail_seg->blocks, 0,
			lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// Initialize segment summary table
	init_seg_summary();

	return 0;
}

void findNextAvailbleSegment() {
	int pointer = tail_seg->seg_num + 1;
	uint8_t *ptr = segUsageTable;
	while (pointer != tail_seg->seg_num) {
		ptr = ptr + pointer;
		if (*ptr == 0) {
			tail_seg->seg_num = pointer;
			printf(
					"SCREENING SEGMENT %d FOUND FOR NEXT AVAILABLE SEGMENT! - ATTEMPTS: %u\n",
					pointer, *ptr);
			return;
		}
		pointer++;
		pointer %= lfs_sb->seg_num;
	}
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Free
 *
 *  This function frees a block with given logAddress
 *
 * Parameters:
 *      logAddress: the address indicates the block to be free.
 *		length: 	the number of bytes to free
 *  
 * Return: 0 - success, 1 - failure
 *
 *----------------------------------------------------------------------
 */
int Log_Free(struct addr *logAddress, int length) {
	// 	number of sectors needed to be freed
	int n_sector = length / FLASH_SECTOR_SIZE;
	if (length % FLASH_SECTOR_SIZE != 0)
		n_sector++;
	// 	Computer starting position
	int start_FlashBlock = logAddress->seg_num * lfs_sb->seg_size
			* lfs_sb->b_size / 16;
	//	Call Flash driver to erase
	Flash_Erase(flash, start_FlashBlock, n_sector);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Indirection Block
 *
 *----------------------------------------------------------------------
 */
// update the block in the ifile, return the block
void updateInode(int inum, int block, struct addr *block_addr, int length) {
	printf("UPDATING INODE OF FILE %d WITH SEG_NUM: %u AND BLOCK_NUM: %u\n",
			inum, block_addr->seg_num, block_addr->block_num);
	// 1. Find the block in ifile where the inode locates.
	void *buffer = malloc(lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// find the block number of ifile:
	int blk_num = inum * sizeof(struct inode)
			/ (lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// read the necessary file block from ifile for reading the inode given the inum
	if (Log_Read(&cp_region->ifile_inode.ptrs[blk_num],
			lfs_sb->b_size * FLASH_SECTOR_SIZE, buffer) == -1) { // get data of the ifile block
		printf("updateInode: Failed to read target block of iFile.\n");
	}
	// 2. Find inode with inum in ifile.
	int offset = inum * sizeof(struct inode) % s_block_byte;
	struct inode *i_inode = (struct inode *) (buffer + offset);
	// 3. Update the address of the file block "block"
	// update static block address
	if (block < 4) {
		i_inode->ptrs[block].seg_num = block_addr->seg_num;
		i_inode->ptrs[block].block_num = block_addr->block_num;
	} else { // update indirect block address
		printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ Writing indirect bolock .... with length %d \n", length);
		// 1. Modify indirect block
		void* indirect_blk = malloc(s_block_byte);
		if(i_inode->indirect.seg_num == LFS_UNUSED_ADDR){
			struct addr *address = malloc(sizeof(struct addr));
			createIndirectBlock(inum, address);
			i_inode->indirect.seg_num = address->seg_num;
			i_inode->indirect.block_num = address->block_num;
			free(address);
		}
		Log_Read(&(i_inode->indirect), s_block_byte, indirect_blk);
		struct addr *t_indirect_blk = indirect_blk
				+ (block * sizeof(struct addr));
		t_indirect_blk->seg_num = block_addr->seg_num;
		t_indirect_blk->block_num = block_addr->block_num;
		// 2. Write indirect block to Flash
		struct addr *idr_addr = malloc(sizeof(struct addr));
		Log_Write(inum, -block, s_block_byte, indirect_blk, idr_addr);
		// 3. Update indirect block address in inode
		i_inode->indirect.seg_num = idr_addr->seg_num;
		i_inode->indirect.block_num = idr_addr->block_num;

	}
	i_inode->size += length; // length > 0 if the size increase; length < 0 if the size decrease;
	i_inode->lst_mdf = time(NULL);
	// writing the file block of ifile in the log.
	//print_inode(i_inode);
	Log_Write(LFS_IFILE_INUM, blk_num, s_block_byte, buffer,
			&(cp_region->ifile_inode.ptrs[blk_num]));
	cp_region->ifile_inode.size =
			cp_region->ifile_inode.size < (inum + 1) * sizeof(struct inode) ?
					(inum + 1) * sizeof(struct inode) :
					cp_region->ifile_inode.size;
	printf("iFile has been updated with FILE %d :\n", inum);
	print_inode(&(cp_region->ifile_inode));
}

void getAddr(int inum, int i, struct addr *address) {
	printf("$$$$$$$$$$$$$$$$$$GETTING ADDRESS OF FILE %d BLOCK %d\n",inum, i);
	struct inode *node = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, node);
	if (i < 4) {
		address->seg_num = node->ptrs[i].seg_num;
		address->block_num = node->ptrs[i].block_num;
	} else {
		if(node->indirect.seg_num == LFS_UNUSED_ADDR){
			address->seg_num = LFS_UNUSED_ADDR;
			address->block_num = LFS_UNUSED_ADDR;
		}else{
			void* indirect_blk = malloc(s_block_byte);
			Log_Read(&(node->indirect), s_block_byte, indirect_blk);
			struct addr *inode_addr = indirect_blk + i * sizeof(struct addr);
			address->seg_num = inode_addr->seg_num;
			address->block_num = inode_addr->block_num;
		}
	}
	free(node);
}
void createIndirectBlock(int inum, struct addr *address){
	void* indirect_blk = malloc(s_block_byte);
	memset(indirect_blk, 0xffff, s_block_byte);
	Log_Write(inum, -1, s_block_byte, indirect_blk, address);
}

/*
 *----------------------------------------------------------------------
 *
 * Segment Cleaner
 *
 *----------------------------------------------------------------------
 */
void cleaner_start() {
	if (free_segment_counter > 4)
		return;
	printf("Clean Engine STARTS>>>>>>>>>>>>>>>>>>>>>>>>\n");
	while (free_segment_counter < 8) {
		if (needClean(cleaner_pointer) == 0) {
			printf("<><><><> -SEGMENT %d HAS BEEN CLEANED | FREE SEGMENTS: %d/%d\n",
								cleaner_pointer, free_segment_counter,
								(int) lfs_sb->seg_num - SUPERBLOCK_SEG_SIZE);
			cleaner(cleaner_pointer);
		} else
			printf("<><><><> -SEGMENT %d DONES'T NEED TO BE CLEANED\n",
					cleaner_pointer);
		cleaner_pointer++;
		cleaner_pointer %= lfs_sb->seg_num;
		if(cleaner_pointer < SUPERBLOCK_SEG_SIZE) cleaner_pointer = SUPERBLOCK_SEG_SIZE;
	}
}
void cleaner(int seg_num) {
	// Read the segment
	void *segment = malloc(s_segment_byte);
	struct addr *seg_addr = malloc(sizeof(struct addr));
	seg_addr->seg_num = seg_num;
	seg_addr->block_num = 0;
	Log_Read(seg_addr, s_segment_byte, segment);
	// Mark the segment as dead
	update_segment_usage_table(seg_addr->seg_num, 1);
	// Free old segment in Flash
	Log_Free(seg_addr, s_segment_byte);
	//Disassemble blocks
	for (int i = size_seg_summary; i < lfs_sb->seg_size; i++) {
		int inum = get_block_inum(segment, i);
		struct addr *blk_addr = malloc(sizeof(struct addr));
		blk_addr->seg_num = seg_num;
		blk_addr->block_num = i;
		int blk_num = 0;
		if ((blk_num = isBlockAlive(inum, blk_addr)) >= 0) {
			Log_Write(inum, blk_num, s_block_byte, segment + i * s_block_byte,
					blk_addr);
		}
		free(blk_addr);
	}
	free_segment_counter++;
}
// 0 - need clean; -1 - no need clean
int needClean(int seg_num) {
	// Read the segment
	void *segment = malloc(s_segment_byte);
	struct addr *seg_addr = malloc(sizeof(struct addr));
	seg_addr->seg_num = seg_num;
	seg_addr->block_num = 0;
	Log_Read(seg_addr, s_segment_byte, segment);
	uint16_t *temp = segment;
	//printf("...........................%u\n", *temp);
	//Disassemble blocks
	for (int i = size_seg_summary; i < lfs_sb->seg_size; i++) {
		int inum = get_block_inum(segment, i);
		struct addr *blk_addr = malloc(sizeof(struct addr));
		blk_addr->seg_num = seg_num;
		blk_addr->block_num = i;
		int blk_num = 0;
		if ((blk_num = isBlockAlive(inum, blk_addr)) < 0) {
			return 0;
		}
		free(blk_addr);
	}
	return -1;
}
// Alive - 0;
int isBlockAlive(int inum, struct addr *seg_addr) {
	struct inode *node = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, node);
	// Check direct pointers
	for (int i = 0; i < 4; i++) {
		if (isTwoAddressSame(seg_addr, &(node->ptrs[i])) == 0)
			return i;
	}
	// TODO check indirect pointers
	for(int i = 4; i < s_block_byte / sizeof(struct addr); i ++){
		struct addr *address = malloc(sizeof(struct addr));
		getAddr(inum, i, address);
		if (isTwoAddressSame(seg_addr, address) == 0){
			free(address);
			return i;
		}
		free(address);
	}
	// Return
	return -1;

}
int isTwoAddressSame(struct addr *addr1, struct addr *addr2) {
	printf("................COMPARING ADDRESSES %u %u AND %u %u\n", addr1->seg_num, addr1->block_num, addr2->seg_num, addr2->block_num);
	if (addr1->seg_num != addr2->seg_num)
		return -1;
	if (addr1->block_num != addr2->block_num)
		return -1;
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Segment Summary Table
 *
 *----------------------------------------------------------------------
 */
void init_seg_summary() {
	int total = size_of_seg_cache * lfs_sb->b_size * FLASH_SECTOR_SIZE / 16;
	uint16_t t = 0xffff;
	for (int i = 0; i < total; i++) {
		memcpy(tail_seg->blocks + i * 2, &t, sizeof(uint16_t));
	}
	next_block_in_tail = size_seg_summary;
}
void set_seg_summary(int block_num, uint16_t inum) {
	memcpy(tail_seg->blocks + block_num * 2, &inum, sizeof(inum));
}
uint16_t get_block_inum(void* segment, int index) {
	uint16_t *temp = segment + index * sizeof(uint16_t);
	uint16_t inum = *temp;
	printf("\\\\\\\\ get block inum %d in summary table returns %u\n", index, inum);
	return inum;
}

/*
 *----------------------------------------------------------------------
 *
 * Segment Usage Table
 *
 *----------------------------------------------------------------------
 */
void load_segment_usage_table() {
	// 	num_of_block_seg_usage_table represents number of blocks being reserved for segment usage table
	segUsageTable = malloc(lfs_sb->seg_num);
	Log_Read(&(lfs_sb->seg_usage_table_addr), lfs_sb->seg_num, segUsageTable);
	for (int i = SUPERBLOCK_SEG_SIZE; i < lfs_sb->seg_num; i++) {
		uint8_t *temp = segUsageTable + i;
		printf("++++++%u\n", *temp);
		if (*temp == 0)
			free_segment_counter++;
	}
	printf("............................Available Free Segments: %d/%d\n",
			free_segment_counter,
			(int) (lfs_sb->seg_num) - SUPERBLOCK_SEG_SIZE);
}
void update_segment_usage_table(int seg_num, int isUnvailble) {
	memset(segUsageTable + seg_num, isUnvailble, sizeof(uint8_t));
}
void write_segment_usage_table() {
	int start_sector = (lfs_sb->seg_usage_table_addr.seg_num * lfs_sb->seg_size
			+ lfs_sb->seg_usage_table_addr.block_num) * lfs_sb->b_size;
	int num_sector = sizeof(segUsageTable) / FLASH_SECTOR_SIZE
			+ (sizeof(segUsageTable) % FLASH_SECTOR_SIZE != 0 ? 1 : 0);
	Flash_Write(flash, start_sector, num_sector, segUsageTable);
}

/*
 *----------------------------------------------------------------------
 *
 * Associative Functions
 *
 *  This part consists of functions that are used for general-purpose
 *
 *----------------------------------------------------------------------
 */
void update_sb() {
	Flash_Write(flash, 0, lfs_sb->seg_size * lfs_sb->b_size * lfs_sb->seg_num,
			lfs_sb);
}

int logAddr_To_Sectors(struct addr *addr) {
	return addr->seg_num * lfs_sb->seg_size * lfs_sb->b_size
			+ addr->block_num * lfs_sb->b_size;
}

int segNum_To_Sectors(uint16_t seg_num) {
	return seg_num * lfs_sb->seg_size * lfs_sb->b_size;
}
int LFS_SEG(int x) {
	return x + SUPERBLOCK_SEG_SIZE;
}

/*
 *----------------------------------------------------------------------
 *
 * LinkedList Class
 *
 *  This function implement a LinkedList for Segment Cache
 *
 * Parameters:
 *      int N - Max size of linkedlist
 *  
 * Return: 
 *
 *----------------------------------------------------------------------
 */
int Seg_Cache_init(int N) {
	if (N < 1) {
		printf("ERROR: Max size of segment cache must be greater than 0!\n");
		return -1;
	}
	max_size_seg_cache = N;

	head = malloc(sizeof(struct LinkedList));
	head->seg_num = LFS_UNUSED_ADDR;

	tail = malloc(sizeof(struct LinkedList));
	tail->seg_num = LFS_UNUSED_ADDR;

	head->next = tail;
	tail->prev = head;

	return 0;
}

void SC_trim() {
	while (size_of_seg_cache > max_size_seg_cache) {
		size_of_seg_cache--;
		tail->prev = tail->prev->prev;
		tail->prev->next = tail;
		printf(
				">>>>>>>A SEGMENT HAS BEEN POPPED OUT FROM {SEGMENT CACHE}: %u!\n",
				tail->prev->seg_num);
	}
}
// Insert a segment onto linkedlist
int SC_push() {
	printf(">>>>>>>PUSHING A SEGMENT INTO {SEGMENT CACHE}: %u!\n",
			tail_seg->seg_num);
	// Init a new LinkedList node
	struct LinkedList *list_node = malloc(sizeof(struct LinkedList));
	list_node->seg_num = tail_seg->seg_num;
	list_node->segment = malloc(
			lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	memcpy(list_node->segment, tail_seg->blocks,
			lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	// Push onto top
	list_node->prev = head;
	list_node->next = head->next;
	head->next->prev = list_node;
	head->next = list_node;
	size_of_seg_cache++;
	SC_trim();
	return 0;
}
// Get a segment from linkedlist with a given address
int SC_get(uint16_t segment_num, void *buffer) {
	printf(">>>>>>>GETTING A SEGMENT FROM {SEGMENT CACHE}: %u\n", segment_num);
	struct LinkedList *ptr = head->next;
	while (ptr->seg_num != LFS_UNUSED_ADDR) {
		if (ptr->seg_num == segment_num) {
			memcpy(buffer, ptr->segment,
					lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
			// Push ptr onto the top
			// remove ptr in the middle of linkedlist
			ptr->next->prev = ptr->prev;
			ptr->prev->next = ptr->next;
			ptr->prev = head;
			ptr->next = head->next;
			// Promote ptr onto head
			head->next->prev = ptr;
			head->next = ptr;
			printf(">>>>>>>RETURNING OUT A SEGMENT FROM {SEGMENT CACHE}: %u!\n",
					ptr->seg_num);
			return 0;
		}
		ptr = ptr->next;
	}
	return -1;
}

void SC_print() {
	printf("\t----- SEGMENT CACHE STATUS -----\n");
	struct LinkedList *ptr = head->next;
	int counter = 0;
	while (ptr->seg_num != LFS_UNUSED_ADDR) {
		printf("\t NUM: %d \t: SEG_NUM: %u \n", counter, ptr->seg_num);
		ptr = ptr->next;
		counter++;
	}
	printf("\t--------------------------------\n");
}

