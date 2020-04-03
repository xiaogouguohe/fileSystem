//global.h
#ifndef  GLOBAL_H
#define  GLOBAL_H

#include <stddef.h>
#include <stdio.h>
extern char *vdisk_path;
extern FILE *fp;

#define FS_BLOCK_SIZE 512
#define FS_DISK_SIZE 5242880

#define SUPER_BLOCK 1
#define BITMAP_BLOCK 1280
#define ROOT_DIR_BLOCK 1

#define MAX_DATA_IN_BLOCK 496
#define MAX_DIR_IN_BLOCK 8
#define MAX_FILENAME 8
#define MAX_EXTENSION 3
#define TOTAL_BLOCK_NUM 10240

struct super_block { //24 bytes
	long fs_size; //sizes of file system, in blocks
	long first_blk; //first block of root directory
	long bitmap; //size of bitmap,in blocks
};

struct u_fs_file_directory { //64 bytes
	char fname[MAX_FILENAME + 1]; //filename(plus space for nul)
	char fext[MAX_EXTENSION + 1]; //extension(plus space for nul)
	long fsize;                 //file size
	long nStartBlock;           //where the first block is on disk
	int flag;  //indicate type of file. 0:for unused.1:for file. 2:for directory
};

struct u_fs_disk_block { // 512 bytes
	int size; //how many bytes are bveing used in this block
	//The next idsk block,if needed This is the next pointer in the linked allocation list
	long nNextBlock;
	//And all the rest of the space in the block can be used for actual data sotorage.
	char data[MAX_DATA_IN_BLOCK];
};

#endif /* EXAMPLE_U_FS_UFS_GLOBAL_H_ */
