#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "lfs.h"
#include "log.h"
#include "flash.h"



struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;

void init()
{
	u_int *blocks_n = malloc(sizeof(u_int));
	flash = Flash_Open("file", FLASH_ASYNC, blocks_n);
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

void print_sb()
{
	printf("seg_size: %u\n", lfs_sb->seg_size);
	printf("b_size: %u\n", lfs_sb->b_size);
	printf("seg_num: %u\n", lfs_sb->seg_num);
	printf("sb_seg_num: %u\n", lfs_sb->sb_seg_num);
	printf("CK_addr_seg: %u\n", lfs_sb->checkpoint_addr.seg_num);
	printf("CK_addr_blo: %u\n", lfs_sb->checkpoint_addr.block_num);
}

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

static int lfs_getattr(const char* path, struct stat* stbuf){ return 0; }
static int lfs_readlink(const char* path, char* buf, size_t size) {return 0;}
static int lfs_mkdir(const char* path, mode_t mode) {return 0;}
static int lfs_unlink(const char* path) {return 0;}
static int lfs_rmdir(const char* path) {return 0;}

static int lfs_open(const char* path, struct fuse_file_info* fi) {
	// check for existence and permissions for the file. 
	return 0;
}

static int lfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	// 1. lookup the file in directory layer
	// for phase 1, this only happens in root directory.
	return 0;
}

static int lfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	return 0;
}

static int lfs_statfs(const char* path, struct statvfs* stbuf){return 0;}
static int lfs_release(const char* path, struct fuse_file_info *fi){return 0;}
static int lfs_opendir(const char* path, struct fuse_file_info* fi){return 0;}
static int lfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){return 0;}
static int lfs_releasedir(const char* path, struct fuse_file_info *fi){return 0;}
static void* lfs_init(struct fuse_conn_info *conn){return 0;}
static void lfs_destroy(void* private_data){}
static int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi){return 0;}
static int lfs_link(const char* from, const char* to){return 0;}
static int lfs_symlink(const char* to, const char* from){return 0;}
static int lfs_truncate(const char* path, off_t size){return 0;}
static int lfs_rename(const char* from, const char* to){return 0;}
static int lfs_chmod(const char* path, mode_t mode){return 0;}
static int lfs_chown(const char* path, uid_t uid, gid_t gid){return 0;}


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
	.chown = lfs_chown
};


int main(int argc, char *argv[])
{
	char 	**nargv = NULL;
    int		i;
    int         nargc;

#define NARGS 3

    nargc = argc + NARGS;

    nargv = (char **) malloc(nargc * sizeof(char*));
    nargv[0] = argv[0];
    nargv[1] = "-f";
    nargv[2] = "-s";
    nargv[3] = "-d";
    for (i = 1; i < argc; i++) {
	nargv[i+NARGS] = argv[i];
    }
    return fuse_main(nargc, nargv, &lfs_oper, NULL);
}


// int main(int argc, char const *argv[])
// {
// 	init();
// 	print_sb();
	
// 	char str[3000];
// 	for(int i = 0; i < 3000; i ++) 
// 		str[i] = 'a';

// 	for (int i = 0; i < 40; ++i)
// 	{
// 		File_Create(i+1, LFS_FILE_TYPE_FILE);
// 	}

// 	for (int i = 0; i < 40; ++i)
// 	{
// 		// char a = (char) i+48;
// 		File_Write(i+1, 0, 3000, &str);
// 	}

// 	for (int i = 0; i < 40; ++i)
// 	{
// 		void *result = malloc(2);
// 		File_Read(i+1, 0, 1, result);
// 		printf("RESULT File %d\n: %s\n", i, (char *) result);
// 	 }

// 	print_cp_region();

// 	if (Flash_Close(flash) != 0) {
// 		printf("ERROR: %s\n", strerror(errno));
// 	}
// 	return 0;
// }