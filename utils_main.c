//utils_main.c

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *vdisk_path = "/home/xiaogouguohe/test_fs2/diskimg";
FILE *fp;

int main()
{
	fp = fopen(vdisk_path, "r+");
	if (!fp)
	{
		printf("open error\n");
		return -1;
	}
	printf("open successfully!\n");

	char cmd[80];
	while (1)
	{
		printf("please print operation: ");
		scanf("%s", cmd);
		if (strcmp(cmd, "init") == 0)
		{
			init_sb_bitmap_data_blocks();	
		}
		else if (strcmp(cmd, "create") == 0)
		{
			char path[80];
			int flag;
			printf("please print path: ");
			scanf("%s", path);
			printf("please print flag: 1 for file, 2 for dir: ");
			scanf("%d", &flag); 
			utils_create(path, flag);
		}
		else if (strcmp(cmd, "write") == 0)
		{
			char path[80], buf[80];
			size_t size, offset;
			printf("please print path: ");
			scanf("%s", path);
			printf("please print buf: ");
			scanf("%s", buf);
			printf("please print size: ");
			scanf("%ld", &size);
			printf("please print offset: ");
			scanf("%ld", &offset);
			utils_write(path, buf, size, offset);
		}
		else if (strcmp(cmd, "show_block") == 0) 
		{
			long blk;
			printf("please print blk: ");
			scanf("%ld", &blk);
			struct u_fs_disk_block* blkinfo = malloc(FS_BLOCK_SIZE);
			if (get_blkinfo_by_blkno(blk, blkinfo) == -1)
			{
				printf("fail to get blkinfo\n");
				continue;
			}
			printf("blkinfo->size = %d\n, blkinfo->nNextBlock = %ld\n, blkinfo->data = %s\n", 
				blkinfo->size, blkinfo->nNextBlock, blkinfo->data);
			
		}
		else if (strcmp(cmd, "exit") == 0)
		{
			break;
		}
	}
	return 0;
}
