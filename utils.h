//utils.h
#ifndef UTILS_H
#define UTILS_H

#include "global.h"

#define ERRNO -1


int utils_write(const char *path, const char *buf, size_t size, off_t offset);

int utils_create(const char* path, int flag);

int get_dir_by_path(const char* path, struct u_fs_file_directory* res_dir);

int alloc_blocks(int num, long* start_blk);

int isBlkAvailable(long blk, unsigned char* bitmap);

void setBitmap(long blk, unsigned char* bitmap);

int findParentAndChild(const char* path, char* parent_path, char* child_path);

int get_blkinfo_by_blkno(long blk, struct u_fs_disk_block* blk_info);

int init_sb_bitmap_data_blocks();

#endif
