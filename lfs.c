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



struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;
char *flash_file_name = NULL;

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

	// Compute size of a block in Byte
	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;

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
 * assign_inum
 *
 *  This function assigns a inum to the given filename and update the coresponding directory(TODO root only for phase 1)
 *
 * Parameters:
 *      const char *filename: the given filename.
 *  
 * Return: uint16_t, the assinged new inum
 *
 *----------------------------------------------------------------------
 */
uint16_t assign_inum(const char *filename)
{
	struct inode *root_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(LFS_ROOT_INUM, root_inode);
	// print_inode(root_inode);
	void *root_dir = malloc(root_inode->size + sizeof(struct dir_entry));
	File_Read(LFS_ROOT_INUM, 0, root_inode->size, root_dir);
	struct dir *root_hdr = root_dir;
	printf("Name: %s, size: %d\n", root_hdr->name, root_hdr->size);
	root_hdr->size ++ ;
	struct dir_entry *last_entry = root_dir + root_inode->size - sizeof(struct dir_entry);
	struct dir_entry *new_entry = root_dir + root_inode->size;
	strncpy(new_entry->name, filename, sizeof(new_entry->name));
	new_entry->inum = last_entry->inum + 1;
	File_Write(LFS_ROOT_INUM, 0, root_inode->size + sizeof(struct dir_entry), root_dir);
	printf("DONE updating root\n");
	return new_entry->inum;
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
uint16_t inum_lookup(const char *filename)
{
	struct inode *root_inode = malloc(sizeof(struct inode));
	// get the inode of TODO root directory
    Read_Inode_in_Ifile(LFS_ROOT_INUM, root_inode);
    // now read the whole root directory.
    void *root_buffer = malloc(root_inode->size);
    File_Read(LFS_ROOT_INUM, 0, root_inode->size, root_buffer);
    struct dir *root_dir = root_buffer;
    struct dir_entry *entry_ptr;
    for (int i = 0; i < root_dir->size; ++i)
    {
    	entry_ptr = root_buffer + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
    	printf("Comparing %s %s\n", filename, entry_ptr->name);
    	if(strcmp(filename, entry_ptr->name) == 0){
    		return entry_ptr->inum;
    	}
    	/* code */
    }
    // for default, if the lookup is failed, then return the 0xFFFF as error code.
    return LFS_UNUSED_ADDR;
}

/*
 *----------------------------------------------------------------------
 *
 * lfs_getattr (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_getattr(const char* path, struct stat* stbuf){

	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_ino = 3;
    } else {
    	// deal with all files in root path. 
    	// first get the inum of the file provided with the file name.(path+1)
    	uint16_t inum = inum_lookup(path+1);
    	printf("GETTING inum in getattr %s %u\n", path, inum);
    	if(inum == LFS_UNUSED_ADDR){
    		res = -ENOENT;
    		return res;
    	}
    	struct inode *inode = malloc(sizeof(struct inode));
    	Read_Inode_in_Ifile(inum, inode);
    	mode_t mode = S_IFREG | 0777;
    	if(inode->type == LFS_FILE_TYPE_FILE){
    		mode = S_IFREG | 0777;
    		stbuf->st_size = inode->size;
    	}
    	if(inode->type == LFS_FILE_TYPE_DIR)
    		mode = S_IFDIR | 0777;
    	stbuf->st_mode = mode;
    	stbuf->st_nlink = 1; // TODO this is hard coded for all files in root directory
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
 *
 *----------------------------------------------------------------------
 */
static int lfs_readlink(const char* path, char* buf, size_t size) {return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_mkdir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_mkdir(const char* path, mode_t mode) {return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_unlink (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_unlink(const char* path) {return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_rmdir (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_rmdir(const char* path) {return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_open (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_open(const char* path, struct fuse_file_info* fi) {
	if(inum_lookup(path+1) == LFS_UNUSED_ADDR){
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
    uint16_t inum = inum_lookup(path+1);
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
    uint16_t inum = inum_lookup(path+1);
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

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	struct inode *root_inode = malloc(sizeof(struct inode));
	Read_Inode_in_Ifile(LFS_ROOT_INUM, root_inode);
	void *root_buffer = malloc(root_inode->size);
	File_Read(LFS_ROOT_INUM, 0, root_inode->size, root_buffer);
	struct dir *root = root_buffer;
	int num_entries = root->size;
	printf("DIR: %s\n", root->name);
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

	// 1. write the most recent checkpoint region. 
	cp_region->last_seg_addr.seg_num = tail_seg->seg_num;
	void *cp_buffer = malloc(s_block_byte);
	memcpy(cp_buffer, cp_region, sizeof(struct checkpoint_region));
	// block=0 is an exception for the situation.
	Log_Write(-1, 0, s_block_byte, cp_buffer, &(lfs_sb->checkpoint_addr));

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
	// assign inum to the given filename, then call File_Create in file layer. 
	uint16_t inum = assign_inum(path+1);
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
static int lfs_link(const char* from, const char* to){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_symlink (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_symlink(const char* to, const char* from){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_truncate (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_truncate(const char* path, off_t size){
	uint16_t inum = inum_lookup(path+1);
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
static int lfs_rename(const char* from, const char* to){return 0;}

/*
 *----------------------------------------------------------------------
 *
 * lfs_chmod (FUSE FUNCTION IMPLEMENTATION)
 *
 *----------------------------------------------------------------------
 */
static int lfs_chmod(const char* path, mode_t mode){return 0;}

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



int main(int argc, char *argv[])
{
	char 	**nargv = NULL;
    int		i;
    int         nargc;
    int our_argc = argc - 2;

#define NARGS 3

    int cache = 4;
    int interval = 4;
    int start = 4;
    int stop = 8;

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
				cache = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'i':
				interval = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'c':
				start = (int) strtol(optarg, (char **)NULL, 10);
				break;
			case 'C':
				stop = (int) strtol(optarg, (char **)NULL, 10);
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
