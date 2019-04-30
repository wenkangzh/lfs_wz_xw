/**
*	filename: lfs.c
*	CSC 552 LFS Project Phase 1
*	Wenkang Zhou wenkangzhou@email.arizona.edu
*	Xueheng Wan  wanxueheng@email.arizona.edu
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "lfs.h"
#include "log.h"
#include "flash.h"
#include <unistd.h>

extern int max_size_seg_cache;

struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;
int s_segment_byte = 0;
char *flash_file_name = NULL;

int max_size_seg_cache;
int periodic_cp_interval;

int size_seg_summary; // in blocks

int SUPERBLOCK_SEG_SIZE;

int start_clean;
int stop_clean;

/*
 *----------------------------------------------------------------------
 *
 * init
 *
 *  This function initialize all the data structures that our LFS will use, including 
 *	reading from the flash about super block and the checkpoint region. 
 *
 * Parameters: NOTHING
 *  
 * Return: void.
 *
 *----------------------------------------------------------------------
 */
void init()
{
	u_int *blocks_n = malloc(sizeof(u_int));
	flash = Flash_Open(flash_file_name, FLASH_ASYNC, blocks_n);
	init_sb();

	// Compute size of a block in Byte and a segment in Byte
	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;
	s_segment_byte = lfs_sb->seg_num * s_block_byte;

	// Compute number of blocks per segment that are reversed for segment summary table
	size_seg_summary = lfs_sb->seg_size / (s_block_byte / sizeof(uint16_t));
	if((lfs_sb->seg_size % (s_block_byte / sizeof(uint16_t)) != 0)) size_seg_summary ++;
	printf("```````````````````size_seg_summary %d, %u, %d, %d\n", size_seg_summary, lfs_sb->seg_size, s_block_byte, (int)sizeof(uint16_t));
	next_block_in_tail = size_seg_summary;

	// Set Variable for size of superblock
	SUPERBLOCK_SEG_SIZE = lfs_sb->sb_seg_num;

	// TODO - need to restore information in checkpoint region in memory.
	cp_region = malloc(sizeof(struct checkpoint_region));

	// Log_Read(&(lfs_sb->checkpoint_addr), sizeof(struct checkpoint_region), cp_region);
	u_int sector_offset = logAddr_To_Sectors(&(lfs_sb->checkpoint_addr));
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = sizeof(struct checkpoint_region) / FLASH_SECTOR_SIZE + (sizeof(struct checkpoint_region) % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp = malloc(FLASH_SECTOR_SIZE * sector_n);
	if(Flash_Read(flash, sector_offset, sector_n, temp) == 1)
		printf("ERROR: %s\n", strerror(errno));
	memcpy(cp_region, temp, sizeof(struct checkpoint_region));

	print_cp_region();

	tail_seg = malloc(sizeof(struct segment));
	tail_seg->seg_num = cp_region == NULL ? LFS_SEG(0) : cp_region->last_seg_addr.seg_num + 1; // TODO find next availble segment
	tail_seg->blocks = malloc(lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	memset(tail_seg->blocks, 0, lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE); // clean the memory
	// here the blocks in the tail segment are all allocated memory space.

	// Init Segment Cache
	Seg_Cache_init(max_size_seg_cache);

	//	Init segment usage table
	load_segment_usage_table();

}

/*
 *----------------------------------------------------------------------
 *
 * init_sb
 *
 *  This function initialize the super block structure. 
 *
 * Parameters: NOTHING
 * 
 * Return: void
 *
 *----------------------------------------------------------------------
 */
void init_sb()
{
	lfs_sb = malloc(sizeof(struct superblock));

	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	if(Flash_Read(flash, 0, 64, buffer) == 1)
		printf("ERROR: %s\n", strerror(errno));
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer + sizeof(uint16_t));
	uint16_t *sb_seg_num = (uint16_t*) (buffer + sizeof(uint16_t) * 3);
	int sectors = *seg_size * *b_size * *sb_seg_num;
	void *temp = malloc(sectors * FLASH_SECTOR_SIZE);
	Flash_Read(flash, 0, sectors, temp);
	memcpy(lfs_sb, temp, sizeof(struct superblock));
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Write
 *
 *  This function is for debugging purpose, prints the superblock structure
 *
 * Parameters: NOTHING
 *  
 * Return: void
 *
 *----------------------------------------------------------------------
 */
void print_sb()
{
	printf("--------------------- SB -------------------\n");
	printf("seg_size: %u\n", lfs_sb->seg_size);
	printf("b_size: %u\n", lfs_sb->b_size);
	printf("seg_num: %u\n", lfs_sb->seg_num);
	printf("sb_seg_num: %u\n", lfs_sb->sb_seg_num);
	printf("CK_addr_seg: %u\n", lfs_sb->checkpoint_addr.seg_num);
	printf("CK_addr_blo: %u\n", lfs_sb->checkpoint_addr.block_num);
	printf("--------------------------------------------\n");
}

/*
 *----------------------------------------------------------------------
 *
 * Log_Write
 *
 *  This function is for debugging purpose, prints the checkpoint region structure
 *
 * Parameters: NOTHING
 *  
 * Return: void
 *
 *----------------------------------------------------------------------
 */
void print_cp_region()
{
	printf("---------- CP region ----------\n");
	printf("timestamp: %ld\n", cp_region->timestamp);
	printf("LSA-seg_num: %u\n", cp_region->last_seg_addr.seg_num);
	printf("LSA-block: %u\n", cp_region->last_seg_addr.block_num);
	printf("ifile-inode-inum: %u\n", cp_region->ifile_inode.inum);
	printf("ifile-inode-type: %u\n", cp_region->ifile_inode.type);
	printf("ifile-inode-size: %d\n", cp_region->ifile_inode.size);
	printf("ifile-inode-addr-seg: %u\n", cp_region->ifile_inode.ptrs[0].seg_num);
	printf("ifile-inode-addr-block: %u\n", cp_region->ifile_inode.ptrs[0].block_num);
	printf("-------------------------------\n");
}

/*
 *----------------------------------------------------------------------
 *
 * print_inode
 *
 *  This function is for debugging purpose, prints the information in given inode.
 *
 * Parameters: 
 *		struct inode *inode: the inode to be printed 
 *  
 * Return: void
 *
 *----------------------------------------------------------------------
 */
void print_inode(struct inode *inode)
{
	printf("\t----- inode -----\n");
	printf("\t inum: %u\n", inode->inum);
	printf("\t type: %u\n", inode->type);
	printf("\t links: %u\n", inode->n_links);
	printf("\t size: %u\n", inode->size);
	printf("\t addr0: %u %u\n", inode->ptrs[0].seg_num, inode->ptrs[0].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[1].seg_num, inode->ptrs[1].block_num);
	printf("\t addr2: %u %u\n", inode->ptrs[2].seg_num, inode->ptrs[2].block_num);
	printf("\t addr3: %u %u\n", inode->ptrs[3].seg_num, inode->ptrs[3].block_num);
	printf("\t-----------------\n");
}


/*
 *----------------------------------------------------------------------
 *
 * find_last_dir
 *
 *  This function find the inum of the last dir in the given path. 
 *
 * Parameters:
 *      const char *path: the given path.
 *  
 * Return: uint16_t, the found last dir in the path, represented by its inum
 *
 *----------------------------------------------------------------------
 */
uint16_t find_last_dir(const char* path, char *new_dir)
{
	printf("===== find_last_dir(%s)\n", path);
	int i;
	for(i = strlen(path) - 1; i >= 0; i  --){
		if(path[i]  == '/') break;
	}
	if(i == 0){
		strcpy(new_dir, path+1);
		return LFS_ROOT_INUM;
	}

	char* parent_path = malloc(i);
	substring((char*)path, parent_path, 0, i);
	substring((char*)path, new_dir, i+1, strlen(path));
	return  inum_lookup(parent_path);
}


/*
 *----------------------------------------------------------------------
 *
 * inum_lookup
 *
 *  This function given filename, return the inum. (TODO currently only for root directory)
 *
 * Parameters:
 *      const char *filename: the given filename.
 *  
 * Return: uint16_t, the corespoinding inum with the given filename
 *
 *----------------------------------------------------------------------
 */
uint16_t inum_lookup(const char *path)
{
	printf("===== inum_lookup(%s)\n", path);
	uint16_t parent_inum = LFS_ROOT_INUM;

	if(strcmp(path, "/") == 0){
		return parent_inum;
	}

    int j = 1, i = 1;
    for(i = 1; i <= strlen(path); i++){
        if(path[i] == '/' || i ==  strlen(path)){
            char *filename = malloc(i - j);
            substring((char*)path, filename, j, i);
            j = i + 1;
            printf("%s\n", filename);
            parent_inum = inum_lookup_from_parent_inum(filename, parent_inum);
        }
    }
    return parent_inum;

}

uint16_t inum_lookup_from_parent_inum(char *filename, uint16_t parent_inum)
{
	printf("===== inum_lookup_from_parent_inum(%s, %u)\n", filename, parent_inum);
	struct inode *parent_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(parent_inum, parent_inode);
	void *parent_buffer = malloc(parent_inode->size);
	File_Read(parent_inum, 0, parent_inode->size, parent_buffer);
	struct dir *parent_dir = parent_buffer;
	struct dir_entry *entry_ptr;
	for(int i = 0; i < parent_dir->size; i++){
		entry_ptr = parent_buffer + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		printf("Comparing %s %s\n", filename, entry_ptr->name);
		if(strcmp(filename, entry_ptr->name) == 0){
			return entry_ptr->inum;
		}
	}
	return LFS_UNUSED_ADDR;
}


/*
 *----------------------------------------------------------------------
 *
 * assign_inum
 *
 *  This function assigns a inum to the given filename and update the coresponding parent directory(TODO root only for phase 1)
 *
 * Parameters:
 *      const char *filename: the given filename.
 *  
 * Return: uint16_t, the assinged new inum
 *
 *----------------------------------------------------------------------
 */
uint16_t assign_inum(const char *filename, uint16_t dir_inum)
{
	printf("===== assign_inum(%s, %u)\n", filename, dir_inum);

	// get the inode for the given directory inum(parent directory of the filename)
	struct inode *dir_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(dir_inum, dir_inode);
	// read the whole parent directory and allocate space for one more dir_entry
	void *dir_buf = malloc(dir_inode->size + sizeof(struct dir_entry));
	File_Read(dir_inum, 0, dir_inode->size, dir_buf);
	// get the parent directory header
	struct dir *dir_hdr = dir_buf;
	// size + 1
	dir_hdr->size ++ ;
	// find the location of the new entry in the directory buffer.
	struct dir_entry *new_entry = dir_buf + dir_inode->size;
	strcpy(new_entry->name, filename);
	new_entry->inum = cp_region->next_inum++;
	File_Write(dir_inum, 0, dir_inode->size + sizeof(struct dir_entry), dir_buf);
	printf("DONE updating parent directory\n");
	return new_entry->inum;


	// struct inode *root_inode = malloc(sizeof(struct inode));
	// Read_Inode_in_Ifile(dir_inum, root_inode);
	// // print_inode(root_inode);
	// void *root_dir = malloc(root_inode->size + sizeof(struct dir_entry));
	// File_Read(dir_inum, 0, root_inode->size, root_dir);
	// struct dir *root_hdr = root_dir;
	// printf("Name: %s, size: %d\n", root_hdr->name, root_hdr->size);
	// root_hdr->size ++ ;
	// struct dir_entry *last_entry = root_dir + root_inode->size - sizeof(struct dir_entry);
	// struct dir_entry *new_entry = root_dir + root_inode->size;
	// strncpy(new_entry->name, filename, sizeof(new_entry->name));
	// new_entry->inum = last_entry->inum + 1;
	// File_Write(dir_inum, 0, root_inode->size + sizeof(struct dir_entry), root_dir);
	// printf("DONE updating root\n");
	// return new_entry->inum;
}

void update_cp_region(){
	// 1. write the most recent checkpoint region. 
	cp_region->last_seg_addr.seg_num = tail_seg->seg_num;
	void *cp_buffer = malloc(s_block_byte);
	memcpy(cp_buffer, cp_region, sizeof(struct checkpoint_region));
	// block=0 is an exception for the situation.
	Log_Write(-1, 0, s_block_byte, cp_buffer, &(lfs_sb->checkpoint_addr));
	// Update Segment Usage Table
	write_segment_usage_table();
	printf("~~~~~~~~~~~~~~~A NEW CHECKPOINTING REGION HAS BEEN ESTABLISHED!~~~~~~~~~~~~~~~\n");
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_getattr (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_getattr(const char* path, struct stat* stbuf){

	printf("===== lfs_getattr(%s)\n", path);
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_ino = 3;
    } else {
    	// deal with all files in root path. 
    	// first get the inum of the file provided with the file name.(path+1)
    	uint16_t inum = inum_lookup(path);
    	printf("GETTING inum in getattr %s %u\n", path, inum);
    	if(inum == LFS_UNUSED_ADDR){
    		res = -ENOENT;
    		return res;
    	}
    	struct inode *inode = malloc(sizeof(struct inode));
    	Read_Inode_in_Ifile(inum, inode);
    	if(inode->type == LFS_FILE_TYPE_FILE){
    		printf("\t (This is a file)\n");
    		stbuf->st_size = inode->size;
    	}
    	else if(inode->type == LFS_FILE_TYPE_DIR){
    		printf("\t (This is a dir)\n");
    	}
    	else if(inode->type == LFS_FILE_TYPE_SYMLINK){
    		printf("\t (This is a symbolic link)\n");
    		stbuf->st_size = inode->size;
    	}
    	else{
    		printf("不对啦😱 %u\n", inode->type);
    	}
    	stbuf->st_mode = inode->mode;
    	stbuf->st_mtime = inode->lst_mdf;
    	stbuf->st_nlink = inode->n_links; 
    	stbuf->st_ino = inode->inum;
    }

    printf("~~~MODE: %lo\n", (long unsigned int) stbuf->st_mode);
    printf("~~~NLINK: %hu\n", stbuf->st_nlink);
    printf("~~~SIZE : %lld\n", stbuf->st_size);
    printf("~~~INUM : %llu\n", stbuf->st_ino);

	return res; 
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_readlink (FUSE FUNCTION IMPLEMENTATION)
 * TODO : Notice only works in root. 
 *
 *----------------------------------------------------------------------
 */
static int lfs_readlink(const char* path, char* buf, size_t size) {
	printf("===== lfs_readlink(%s, %lu)\n", path, size);
	uint16_t link_inum = inum_lookup(path);
	struct inode *link_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(link_inum, link_inode);
	void *link_buffer = malloc(link_inode->size);
	File_Read(link_inum, 0, link_inode->size, link_buffer);
	printf("\t BUFFER: %s\n",(char *) link_buffer);
	uint16_t real_file_inum = inum_lookup((char *)link_buffer);
	printf("\t REAL FILE: %u\n", real_file_inum);
	// File_Read(real_file_inum, 0, size, buf);
	strcpy(buf, link_buffer);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_mkdir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_mkdir(const char* path, mode_t mode) {
	printf("===== lfs_mkdir(%s)\n", path);
	// the given directory name is created under 
	char new_dir[LFS_FILE_NAME_MAX_LEN];
	memset(new_dir, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t parent_inum = find_last_dir(path, new_dir);
	printf("PARENT: %u - NEW DIR: %s\n", parent_inum, new_dir);
	// TODO need a new inum for the new directory.
	uint16_t new_inum = assign_inum(new_dir, parent_inum);
	// need to create the new directory file through File layer.
	File_Create(new_inum, LFS_FILE_TYPE_DIR);
	// prepare the initial value of a directory, . and ..
	void *dir_buf = malloc(sizeof(struct dir) + 2 * sizeof(struct dir_entry));
	struct dir *dir_hdr = dir_buf;
	strcpy(dir_hdr->name, new_dir);
	dir_hdr->size = 2;
	struct dir_entry *temp = dir_buf + sizeof(struct dir);
	strcpy(temp->name, ".");
	temp->inum = new_inum;
	temp = dir_buf + sizeof(struct dir) + sizeof(struct dir_entry);
	strcpy(temp->name, "..");
	temp->inum = parent_inum;
	File_Write(new_inum, 0, sizeof(struct dir) + 2 * sizeof(struct dir_entry), dir_buf);

	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_unlink (FUSE FUNCTION IMPLEMENTATION)
 * 
 * Remove (delete) the given file, symbolic link, hard link, or special node—
 * but NOT a directory. Note that if you support hard links, unlink only
 * deletes the data when the last hard link is removed.
 * 
 * Trick: in order to change the content in directory file, we create a new directory file using File_Create with the same inum,
 * then write the new data in the file.
 *
 *----------------------------------------------------------------------
 */
static int lfs_unlink(const char* path) {
	printf("===== lfs_unlink(%s) \n", path);
	uint16_t inum = inum_lookup(path);
	struct inode *inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode);

	if(inode->type == LFS_FILE_TYPE_FILE && inode->n_links <= 1){
		// need to actually free the file
		File_Free(inum);
	}

	// then delete the parent directory entry here. 
	char filename[LFS_FILE_NAME_MAX_LEN];
	uint16_t parent_inum = find_last_dir(path, filename);
	struct inode *parent_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(parent_inum, parent_inode);
	void *old_parent_dir_buf = malloc(parent_inode->size);
	File_Read(parent_inum, 0, parent_inode->size, old_parent_dir_buf);

	void *new_parent_dir_buf = malloc(parent_inode->size - sizeof(struct dir_entry));
	// copy the header
	memcpy(new_parent_dir_buf,old_parent_dir_buf, sizeof(struct dir));
	struct dir *new_ptr = new_parent_dir_buf;
	new_ptr->size --;

	struct dir *old_hdr = old_parent_dir_buf;
	// now iterate through the old parent directory, then copy if needed
	struct dir_entry *old_ptr;
	int j = 0; // counter of new directory file
	for(int i = 0; i < old_hdr->size; i++){
		old_ptr = old_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		// if it is not the deleted file, then copy into the new directory file
		if(strcmp(filename, old_ptr->name) != 0){
			// find the new destination. 
			new_ptr = new_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * j);
			memcpy(new_ptr, old_ptr, sizeof(struct dir_entry));
			j++;
		}
	}

	// after the loop, new_parent_dir_buf has the correct dir file content
	File_Create(parent_inum, LFS_FILE_TYPE_DIR);
	File_Write(parent_inum, 0, parent_inode->size - sizeof(struct dir_entry), new_parent_dir_buf);

	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_rmdir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_rmdir(const char* path) {
	// ENOTEMPTY
	printf("===== lfs_rmdir(%s)\n", path);

	// first check if the given directory is empty or not. 
	uint16_t inum = inum_lookup(path);
	struct inode *inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode);
	void *dir_buf = malloc(inode->size);
	File_Read(inum, 0, inode->size, dir_buf);
	struct dir *hdr = dir_buf;
	printf("\t QQQQQ %s %d\n", hdr->name, hdr->size);
	if(hdr->size > 2)
		return -ENOTEMPTY;
	// here we free the directory file
	File_Free(inum);
	// now remove the entry from its parent directory
	// get the parent directory file
	char dirname[LFS_FILE_NAME_MAX_LEN];
	memset(dirname, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t parent_inum = find_last_dir(path, dirname);
	struct inode *parent_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(parent_inum, parent_inode);
	void *old_parent_dir_buf = malloc(parent_inode->size);
	File_Read(parent_inum, 0, parent_inode->size, old_parent_dir_buf);

	// now we create a new buffer for the new parent directory file
	void *new_parent_dir_buf = malloc(parent_inode->size - sizeof(struct dir_entry));
	// copy the header, decrement the size by 1
	memcpy(new_parent_dir_buf, old_parent_dir_buf, sizeof(struct dir));
	struct dir *new_ptr = new_parent_dir_buf;
	new_ptr->size --;
	struct dir *old_hdr = old_parent_dir_buf;
	// now iterate through the old parent directory, copy the entry if it's not the removed one
	struct dir_entry *old_ptr;
	int j = 0; 	// counter of new directory file
	for(int i = 0; i < old_hdr->size; i++){
		old_ptr = old_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		// if it's not the deleted file, 
		if(strcmp(dirname, old_ptr->name) != 0){
			// find the new destination
			new_ptr = new_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * j);
			memcpy(new_ptr, old_ptr, sizeof(struct dir_entry));
			j++;
		}
	}
	// after the loop, need to create and write new directory file with the same inum
	File_Create(parent_inum, LFS_FILE_TYPE_DIR);
	File_Write(parent_inum, 0, parent_inode->size - sizeof(struct dir_entry), new_parent_dir_buf);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_open (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_open(const char* path, struct fuse_file_info* fi) {
	if(inum_lookup(path) == LFS_UNUSED_ADDR){
		return -ENOENT;
	}
	printf("OPEN 成功˜\n");
	// check for existence and permissions for the file. 
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_read (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	memset(buf, 0, size);
	printf("----- FUSE_READING %s size %ld offset %lld\n", path, size, offset);
	// 1. lookup the file in directory layer
	// for phase 1, this only happens in root directory.
	size_t len;
    (void) fi;
    uint16_t inum = inum_lookup(path);
    struct inode *inode = malloc(sizeof(struct inode));
    Read_Inode_in_Ifile(inum, inode);
    if(inum == LFS_UNUSED_ADDR){
		return -ENOENT;
	}
	print_inode(inode);
	len = inode->size;
	if (offset < len) {
        if (offset + size > len)
            size = len - offset;
    } else{
        size = 0;
    }
	File_Read(inum, offset, size, buf);

	printf("----- READ CONTENT %ld: %s\n",size, buf);
	return size;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_write (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	printf("----- FUSE_写 %s size %ld offset %lld\n", path, size, offset);
    (void) fi;
    uint16_t inum = inum_lookup(path);
    if(inum == LFS_UNUSED_ADDR){
		return -ENOENT;
	}
	File_Write(inum, offset, size, (void *) buf);
	return size;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_statfs (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_statfs(const char* path, struct statvfs* stbuf){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_release (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_release(const char* path, struct fuse_file_info *fi){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_opendir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_opendir(const char* path, struct fuse_file_info* fi){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_readdir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
	// 
	(void) offset;
	(void) fi;

	printf("===== lfs_readdir(%s)\n", path);

	uint16_t inum = inum_lookup(path);

	struct inode *root_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, root_inode);
	printf("^^^^^^^^^^^^^^^^^^^^^^^SIZE: %d\n", root_inode->size);
	void *root_buffer = malloc(root_inode->size);
	File_Read(inum, 0, root_inode->size, root_buffer);
	struct dir *root = root_buffer;
	printf("DIR+++ %s : %u\n", root->name, root->size);
	int num_entries = root->size;
	struct dir_entry *ptr = root_buffer;
	for (int i = 0; i < num_entries; ++i)
	{
		ptr = root_buffer + sizeof(struct dir) + sizeof(struct dir_entry) * (i);
		printf("FILE %u: %s\n", ptr->inum, ptr->name);
		// each time we read a directory entry from the root buffer. 
		filler(buf, ptr->name, NULL, 0);

	}

	// need to read directory file here. 
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_releasedir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_releasedir(const char* path, struct fuse_file_info *fi){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_init (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static void* lfs_init(struct fuse_conn_info *conn){
	init();
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_destroy (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static void lfs_destroy(void* private_data){
	// called when the filesystem exits. 
	printf("EXIT LFS!!!!!!! GOODBYE.再见。\n");	

	//1. write the most recent checkpoint region. 
	update_cp_region();
	// cp_region->last_seg_addr.seg_num = tail_seg->seg_num;
	// void *cp_buffer = malloc(s_block_byte);
	// memcpy(cp_buffer, cp_region, sizeof(struct checkpoint_region));
	// // block=0 is an exception for the situation.
	// Log_Write(-1, 0, s_block_byte, cp_buffer, &(lfs_sb->checkpoint_addr));

	// 2. write the tail segment.
	write_tail_seg_to_flash();

	// 3. write superblock to flash.
	void *sb_buffer = malloc(lfs_sb->seg_size * s_block_byte);
	memcpy(sb_buffer, lfs_sb, sizeof(struct superblock));
	Flash_Erase(flash, 0, lfs_sb->seg_size * lfs_sb->b_size / FLASH_SECTORS_PER_BLOCK);
	Flash_Write(flash, 0, lfs_sb->seg_size * lfs_sb->b_size, sb_buffer);

	// Close the flash.
	if (Flash_Close(flash) != 0) {
		printf("ERROR: %s\n", strerror(errno));
	}
}


/*
 *----------------------------------------------------------------------
 *
 * lfs_create (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi){
	printf("===== lfs_create(%s)\n", path);
	// assign inum to the given filename, then call File_Create in file layer. 
	char *filename = malloc(LFS_FILE_NAME_MAX_LEN);
	uint16_t parent_inum = find_last_dir(path, filename);
	printf("))))))))%s\n", path);
	uint16_t inum = assign_inum(filename, parent_inum);
	File_Create(inum, LFS_FILE_TYPE_FILE);
	struct inode *temp = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, temp);
	print_inode(temp);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_link (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_link(const char* from, const char* to){
	printf("===== lfs_link(%s, %s)\n", from, to);
	char new_file_name[LFS_FILE_NAME_MAX_LEN];
	memset(new_file_name, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t file_inum = inum_lookup(from);
	uint16_t new_parent_inum = find_last_dir(to, new_file_name);
	printf("===== %s %u %u\n", new_file_name, file_inum, new_parent_inum);

	// 1. Add an entry in the desination directory
	struct inode *new_parent_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(new_parent_inum, new_parent_inode);
	// buffer to hold dir file
	void *new_parent_dir_buffer = malloc(new_parent_inode->size + sizeof(struct dir_entry));
	File_Read(new_parent_inum, 0, new_parent_inode->size, new_parent_dir_buffer);
	// get the parent directory header
	struct dir *dir_hdr = new_parent_dir_buffer;
	dir_hdr->size ++;
	// the new entry
	struct dir_entry *new_entry = new_parent_dir_buffer + new_parent_inode->size;
	strcpy(new_entry->name, new_file_name);
	new_entry->inum = file_inum;
	File_Write(new_parent_inum, 0, new_parent_inode->size + sizeof(struct dir_entry), new_parent_dir_buffer);

	// 2. update the inode, increment the number of hard links. 

	struct inode *file_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(file_inum, file_inode);
	file_inode->n_links ++;
	write_inode_in_ifile(file_inum, file_inode);


	// test for the write function
	struct inode *temp = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(file_inum, temp);
	print_inode(temp);
	printf("\t===== lfs_link() returns\n");

	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_symlink (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_symlink(const char* to, const char* from){
	printf("===== lfs_symlink(%s, %s)\n", to, from);
	char link_filename[LFS_FILE_NAME_MAX_LEN];
	memset(link_filename, 0, LFS_FILE_NAME_MAX_LEN);
	// "to" is the linked file, when evaluate "from", it lead to "to"
	uint16_t link_parent_inum = find_last_dir(from, link_filename);
	// this will create a directory entry in the function, and a new inum.
	uint16_t link_inum = assign_inum(link_filename, link_parent_inum); 
	// this will create the inode itself with a file type to be symlink
	File_Create(link_inum, LFS_FILE_TYPE_SYMLINK);
	// the only content in the symlink file is the path to actual file.
	File_Write(link_inum, 0, strlen(to), (void *)to);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_truncate (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_truncate(const char* path, off_t size){
	uint16_t inum = inum_lookup(path);
	struct inode *inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode);
	// Get Data of file_inum
	void *old_buffer = malloc(inode->size);
	File_Read(inum, 0, inode->size, old_buffer);
	// Clear blocks of the file_inum
	for(int i = 3; i >= 0; i --){
		if(inode->ptrs[i].seg_num != LFS_UNUSED_ADDR){
			struct addr *block_addr = malloc(sizeof(struct addr));
			block_addr->seg_num = LFS_UNUSED_ADDR;
			block_addr->block_num = LFS_UNUSED_ADDR;
			updateInode(inum, i, block_addr, - inode->size % s_block_byte);
		}
	}
	// Truncate the file
	void *new_buffer = malloc(size);
	memcpy(new_buffer, old_buffer, inode->size < size ? inode->size : size );
	// Write
	File_Write(inum, 0, size, new_buffer);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_rename (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_rename(const char* from, const char* to){
	printf("===== lfs_rename(%s, %s)\n", from, to);
	char from_filename[LFS_FILE_NAME_MAX_LEN];
	uint16_t parent_inum = find_last_dir(from, from_filename);
	printf("\tfind_last_dir(%s) returned %u\n", from, parent_inum);
	char to_filename[LFS_FILE_NAME_MAX_LEN];
	parent_inum = find_last_dir(to, to_filename);
	printf("\tfind_last_dir(%s) returned %u\n", to, parent_inum);

	struct inode *parent_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(parent_inum, parent_inode);
	void *parent_buffer = malloc(parent_inode->size);
	File_Read(parent_inum, 0, parent_inode->size, parent_buffer);
	struct dir *parent_dir = parent_buffer;

	printf("^^^^^^^^^^^^^^^^^^^^^^^SIZE: %d\n", parent_inode->size);
	struct dir_entry *entry_ptr;
	for(int i = 0; i < parent_dir->size; i++){
		entry_ptr = parent_buffer + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		printf("^^^^^^^^^^^^^^^^^^^^^^^%s\n", entry_ptr->name);
		if(strcmp(from_filename, entry_ptr->name) == 0){
			strcpy(entry_ptr->name, to_filename);
			break;
		}
	}

	File_Write(parent_inum, 0, parent_inode->size, parent_buffer);

	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_chmod (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_chmod(const char* path, mode_t mode){
	printf("===== lfs_chmod(%s)\n", path);
	uint16_t inum = inum_lookup(path);
	struct inode *inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(inum, inode);
	inode->mode = mode;
	write_inode_in_ifile(inum, inode);
	return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_chown (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_chown(const char* path, uid_t uid, gid_t gid){return 0;}


/*
 *----------------------------------------------------------------------
 *
 * fuse operations structure needed by fuse_main to identify the implementation choice for FUSE
 *
 *----------------------------------------------------------------------
 */
static struct fuse_operations lfs_oper = {
	.getattr = lfs_getattr,
	.readlink = lfs_readlink,
	.mkdir = lfs_mkdir,
	.unlink = lfs_unlink,
	.rmdir = lfs_rmdir,
	.open = lfs_open,
	.read = lfs_read,
	.write = lfs_write,
	.statfs = lfs_statfs,
	.release = lfs_release,
	.opendir = lfs_opendir,
	.readdir = lfs_readdir,
	.releasedir = lfs_releasedir,
	.init = lfs_init,
	.destroy = lfs_destroy,
	.create = lfs_create,
	.link = lfs_link,
	.symlink = lfs_symlink,
	.truncate = lfs_truncate,
	.rename = lfs_rename,
	.chmod = lfs_chmod,
	.chown = lfs_chown,
};

void substring(char s[], char sub[], int p, int l) {
   int c = 0;
   while (p + c < l) {
      sub[c] = s[p + c];
      c++;
   }
}



int main(int argc, char *argv[])
{
	char 	**nargv = NULL;
    int		i;
    int         nargc;
    int our_argc = argc - 2;

#define NARGS 3

    max_size_seg_cache = 4;
    periodic_cp_interval = 4;
    start_clean = 4;
    stop_clean = 8;

    int opt = 0;
	struct option long_options[] = {
		{"cache", 		required_argument, 0, 's'}, 
		{"interval", 	required_argument, 0, 'i'},
		{"start", 	required_argument, 0, 'c'},
		{"stop", 	required_argument, 0, 'C'}
	};
	int long_index = 0;
	while ((opt = getopt_long(argc, argv, "s:i:c:C:", 
					long_options, &long_index)) != -1){
		switch (opt) {
			case 's':
				max_size_seg_cache = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'i':
				periodic_cp_interval = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'c':
				start_clean = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'C':
				stop_clean = (int) strtol(optarg, (char **)NULL, 10);
				break;
			default:
				printf("usage: ... \n");
		}
	}


	flash_file_name = argv[argc-2];
    nargc = argc + NARGS - our_argc;

    nargv = (char **) malloc(nargc * sizeof(char*));
    nargv[0] = argv[0];
    nargv[1] = "-f";
    nargv[2] = "-s";
    nargv[3] = "-d";
    for (i = 1; i < argc - our_argc; i++) {
	nargv[i+NARGS] = argv[i+our_argc];
    }
    return fuse_main(nargc, nargv, &lfs_oper, NULL);
}
