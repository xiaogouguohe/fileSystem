//utils.c

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

	
int utils_write(const char *path, const char *buf, size_t size, off_t offset)
{

	//找到path对应的目录项
	struct u_fs_file_directory* file_dir = malloc(sizeof(struct u_fs_disk_block));
	if (get_dir_by_path(path, file_dir) == -1)    
	{
		//printf("666\n");
		return -1;
	}
	if (file_dir->flag != 1)    //不是文件
	{
		return -1;
	}
	
	if (offset > file_dir->fsize)     //偏移量大于文件大小
	{
		free(file_dir);
		return -1;
	}

	long blk = file_dir->nStartBlock;	
	if (file_dir->nStartBlock == -1)    //目录项没有起始块
	{
		free(file_dir);
		return -1;	
	}
	
	//得到起始块信息
	struct u_fs_disk_block* blkinfo = malloc(FS_BLOCK_SIZE);
	if (get_blkinfo_by_blkno(file_dir->nStartBlock, blkinfo) == -1)
	{
		return -1;
	}
	
	//根据offset定位到开始写入的块
	while (offset > blkinfo->size)
	{
		offset -= blkinfo->size;
		blk = blkinfo->nNextBlock;
		if (get_blkinfo_by_blkno(blkinfo->nNextBlock, blkinfo) == -1)
		{
			return -1;
		}
	}

	char* pt = blkinfo->data + offset;
	if (size <= MAX_DATA_IN_BLOCK - offset)    //要写入的大小小于等于该块剩余空间的大小
	{
		printf("blk = %ld, size = %ld, blkinfo->size = %d\n", blk, size, blkinfo->size); 
		memcpy(pt, buf, size);
		blkinfo->size += size;    //该块的已用空间发生变化
		fseek(fp, FS_BLOCK_SIZE * blk, SEEK_SET);
		fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);    //不要忘记写回磁盘
		return 0;
	}
	
	memcpy(pt, buf, MAX_DATA_IN_BLOCK - offset);    
		//要写入的大小大于该块剩余空间的大小（MAX_DATA_IN_BLOCK - offset表示在该块写入的大小）
	blkinfo->size = MAX_DATA_IN_BLOCK;    //该块的已用空间发生变化

	fseek(fp, FS_BLOCK_SIZE * blk, SEEK_SET);
	fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);    //写回磁盘

	size -= MAX_DATA_IN_BLOCK - offset;
	long new_blk;

	while (size)
	{
		if (blkinfo->nNextBlock == -1)    //没有下一个块，需要分配
		{	
			alloc_blocks(1, &new_blk);
			blkinfo->nNextBlock = new_blk;
		}
		
		blk = blkinfo->nNextBlock;		
		if (get_blkinfo_by_blkno(blkinfo->nNextBlock, blkinfo) == -1)
		{
			return -1;
		}

		if (size > MAX_DATA_IN_BLOCK)
		{
			memcpy(blkinfo->data, buf, MAX_DATA_IN_BLOCK);
			blkinfo->size = MAX_DATA_IN_BLOCK;
			size -= MAX_DATA_IN_BLOCK;	
		}
		else
		{
			memcpy(blkinfo->data, buf, size);
			blkinfo->size = size;
			size = 0;
		}
		fseek(fp, 0, SEEK_SET);
		fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);
	}

	return 0;
}

int utils_create(const char* path, int flag)
{
	if (flag != 1 && flag != 2)
	{
		return -1;
	}

	struct u_fs_file_directory* file_dir = malloc(sizeof(struct u_fs_file_directory));
		//用来构建path对应的目录项

	int res = get_dir_by_path(path, file_dir);
	if (res == 0)    //已经存在对应的目录项
	{
		printf("already exist\n");
		free(file_dir);
		return 0;
	}

	if (res == ERRNO)    //出错
	{
		free(file_dir);	
		return ERRNO;
	}
	
	char *q = strchr(path, '/');    //判断path是否有'/'
	if (!q)    //要在根目录下创建path
	{
		//printf("no /\n");

		//if (get_dir_by_path(path, file_dir) == -1)    
			//根目录下不存在path（如果是因出错返回-1，有可能实际上已存在该目录却被判断为不存在），需要创建，这时直接在根目录块创建
		//{
		//printf("not exist\n");
		//初始化目录项
		strcpy(file_dir->fname, path);    
		file_dir->fsize = 0;	
		//file_dir->nStartBlock = -1;    //有问题，不能这么写，要在开始就给目录项分配块（即nStartBlock）

		//分配块给目录项（即nStartBlock）
		long blk;    
		if (alloc_blocks(1, &blk) == ERRNO)    
		{
			free(file_dir);
			return -ERRNO;
		}
		file_dir->nStartBlock = blk;
		file_dir->flag = flag;    //标记为目录或文件

		//把根目录块的内容写入blkinfo
		struct u_fs_disk_block* blkinfo = malloc(sizeof(struct u_fs_disk_block));
		if (get_blkinfo_by_blkno(1281, blkinfo) == -1)
		{
			return ERRNO;
			free(blkinfo);
			free(file_dir);
		}
		blk = 1281;

		while (MAX_DATA_IN_BLOCK - blkinfo->size < sizeof(struct u_fs_file_directory))    
			//找到能写入该目录项的块（块的剩余空间可能不够，需要寻找下一个块即nNextBlock
		{
			blk = blkinfo->nNextBlock;
			///printf("blk = %ld\n", blk);
			if (blkinfo->nNextBlock == -1)    //已经没有下一个块，需要分配块
			{
				long new_blk;
				alloc_blocks(1, &new_blk);
				blk = new_blk;
				//printf("blk = %ld\n", blk);
				if (get_blkinfo_by_blkno(new_blk, blkinfo) == -1)    //根据新块序号得到新块的内容blkinfo
				{
					free(blkinfo);
					free(file_dir);
					return ERRNO;
				}

				//向新块中写入该目录项
				blkinfo->size = sizeof(struct u_fs_file_directory);
				blkinfo->nNextBlock = -1;
				memcpy(blkinfo->data, file_dir, sizeof(struct u_fs_file_directory));

				//新块的内容发生更改，需要更新磁盘
				fseek(fp, FS_BLOCK_SIZE * new_blk, SEEK_SET);
				fwrite(blkinfo, sizeof(struct u_fs_disk_block), 1, fp);

				return 0;    //写完直接返回
			}
			else    //有下一个块
			{
				blk = blkinfo->nNextBlock;    //跳到下一个块
				if (get_blkinfo_by_blkno(blkinfo->nNextBlock, blkinfo) == -1)    //下一个块的内容blkinfo
				{	
					free(blkinfo);
					free(file_dir);
					return -ERRNO;
				}
			}
		}

		//printf("file_dir->fname = %s, file_dir->nStartBlock = %ld\n", 
		//	file_dir->fname, file_dir->nStartBlock);
		memcpy(blkinfo->data + blkinfo->size, file_dir, sizeof(struct u_fs_file_directory));    //把新目录项写入blkinfo->data
		blkinfo->size += sizeof(struct u_fs_file_directory);    //blkinfo的已用大小发生变化

		//blkinfo写回磁盘
		fseek(fp, FS_BLOCK_SIZE * blk, SEEK_SET);
		fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);    
		free(blkinfo);
		free(file_dir);
		return 0;
	} 

	//这是输入的目录中存在'/'的情况
	char parent_path[80], child_path[80];
	findParentAndChild(path, parent_path, child_path);    //分离出最后一个'/'的两部分，然后在parent_path下创建child_path

	if (get_dir_by_path(parent_path, file_dir) != 0)    //file_dir就是parent_path对应的目录项
	{
		free(file_dir);
		return ERRNO;
	}
	//printf("file_dir->fname = %s, file_dir->nStartBlock = %ld\n", file_dir->fname, file_dir->nStartBlock);
	
	struct u_fs_disk_block* blkinfo = malloc(sizeof(struct u_fs_disk_block));

	long blk = file_dir->nStartBlock;    //parent_path的起始块
	if (get_blkinfo_by_blkno(file_dir->nStartBlock, blkinfo) == ERRNO)   
	{
		free(blkinfo);
		free(file_dir);
		return ERRNO;
	}
	
	while (blkinfo->size + sizeof(struct u_fs_file_directory) > MAX_DATA_IN_BLOCK)    
		//从parent_path的起始块开始，找到空间足够写入新目录项的块
	{
		if (blkinfo->nNextBlock == -1)    //没有下一个块，需要创建新块作为下一个块
		{
			long start_blk;
			alloc_blocks(1, &start_blk);
			blkinfo->nNextBlock = start_blk;
		}

		//跳到下一个块
		blk = blkinfo->nNextBlock;
		if (get_blkinfo_by_blkno(blkinfo->nNextBlock, blkinfo) == ERRNO)
		{
			free(blkinfo);
			free(file_dir);
			return ERRNO;
		}
	}
	
	struct u_fs_file_directory* res_dir = malloc(sizeof(struct u_fs_file_directory));    //res_dir是新创建的目录对应的目录项

	q = strchr(child_path, '.');    //注意child_path可能有扩展名（.txt之类的）
	if (q)    //有扩展名
	{
		*q = '\0';
		q = q + 1;
		strcpy(res_dir->fext, q);
	}


	strcpy(res_dir->fname, child_path);

	//新目录项一旦创建，就要有nStartBlock
	long new_blk;
	if (alloc_blocks(1, &new_blk) == ERRNO)
	{
		free(blkinfo);
		free(file_dir);
		free(res_dir);
		return ERRNO;
	}
	res_dir->nStartBlock = new_blk;
	res_dir->fsize = 0;
	res_dir->flag = flag;    //目录或文件

	//printf("res_dir->fname = %s, res_dir->nStartBlock = %ld\n", res_dir->fname, res_dir->nStartBlock);

	memcpy(blkinfo->data + blkinfo->size, res_dir, sizeof(struct u_fs_file_directory));    //res_dir写到blkinfo
	blkinfo->size += sizeof(struct u_fs_file_directory);

	//blkinfo写入磁盘
	fseek(fp, FS_BLOCK_SIZE * blk, SEEK_SET);    
	fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);

	free(blkinfo);
	free(file_dir);
	free(res_dir);
	return 0;
}

int get_dir_by_path(const char* path, struct u_fs_file_directory* res_dir)        
	//根据path找到对应的struct u_fs_file_directory的实例，并写入到res_dir中
{
	struct u_fs_disk_block* blkinfo = malloc(sizeof(struct u_fs_disk_block));
	long blk = 1281;
		
	//拿出超级块

	if (get_blkinfo_by_blkno(1281, blkinfo) == ERRNO)    //通过超级块，拿出根目录块的内容（其实可以直接访问根目录块1281）
	{
		free(blkinfo);
		return ERRNO;
	}

	struct u_fs_file_directory* file_dir = (struct u_fs_file_directory*) blkinfo->data;
		//千万不要认为可以用参数的res_dir完成这个函数的访问某些目录项，一定要另外创建一个


	if (strcmp(path, ".") == 0) { return ERRNO; } 
	else if (strcmp(path, "..") == 0) { return ERRNO; }

	else 
	{
		char *cur = strdup(path);

		int flag = 1;
		while(flag)    //以'/'为断点，一层一层查找
		{
			char *q = strchr(cur, '/');    //找到第一个'/'的位置
			if (!q)    //已经没有'/'了，下次到最外层循环的判断就可以退出了
			{
				//printf("change\n");
				flag = 0;
			}
			else
			{
				*q = '\0';    //分割
				++q;
			}
			
			//printf("cur = %s, q = %s\n", cur, q);
			while(1)    //这层循环用来找某一层（即两个'/'中间的内容）
			{
				int offset = 0;
				if (offset + sizeof(struct u_fs_file_directory) > blkinfo->size)    //需要寻找下一个块
				{	
					blk = blkinfo->nNextBlock;
					if (blkinfo->nNextBlock == -1)    //不存在下一个块，也就是说没找到path对应的目录项
					{					
						free(blkinfo);
						return -2;
					}
					if (get_blkinfo_by_blkno(blkinfo->nNextBlock, blkinfo) == ERRNO)    //找下一个块
					{
						free(blkinfo);
						return ERRNO;
					}
					offset = 0;
				}
				
				file_dir = (struct u_fs_file_directory*) blkinfo->data + offset;    //定位到当前目录项
				offset += sizeof(struct u_fs_file_directory);
				//printf("cur = %s, file_dir->fname = %s\n", cur, file_dir->fname);
				if (strcmp(cur, file_dir->fname) == 0)    //找到
				{
					//printf("333\n");
					memcpy(res_dir, file_dir, sizeof(struct u_fs_file_directory));    //当前目录项赋给res_dir
					//printf("cur = %s, file_dir->fname = %s\n", cur, file_dir->fname);
					
						
					blk = file_dir->nStartBlock;
					//printf("res_dir->nStartBlock = %ld\n", res_dir->nStartBlock);
					if (get_blkinfo_by_blkno(file_dir->nStartBlock, blkinfo) == -1)    
						//这里就是为什么res_dir和file_dir不能混用的原因
					{
						free(blkinfo);
						return -ERRNO;
					}
					cur = q;
					break;
				}
			}
		}			
	}
	//printf("successfully make in blk %ld\n", res_dir->nStartBlock);
	free(blkinfo); 
	return 0;
}	

int alloc_blocks(int num, long* start_blk)
{
	unsigned char bitmap[TOTAL_BLOCK_NUM / (sizeof(unsigned char) * 8)];
	
	printf("open successfully!\n");*/

	fseek(fp, FS_BLOCK_SIZE, SEEK_SET);
	fread(bitmap, sizeof(bitmap), 1, fp);
	/*for (int i = 0; i < 100; ++i)
	{
		printf("bitmap[%d] = %d\n", i, bitmap[i]);
	}*/
	int need = num;
	struct u_fs_disk_block* blkinfo = malloc(sizeof(struct u_fs_disk_block));

	long blk = 0, prev_blk = 0;
	for (; need && blk < TOTAL_BLOCK_NUM; ++blk)
	{
		if (isBlkAvailable(blk, bitmap))
		{
			//printf("blk = %ld, need = %d\n", blk, need);
			setBitmap(blk, bitmap);
			if (need == num)
			{
				*start_blk = blk;
				
			}
			else 
			{
				blkinfo->nNextBlock = blk;

				//被分配的块发生变化，要写回磁盘
				fseek(fp, FS_BLOCK_SIZE * prev_blk, SEEK_SET);
				fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);
			}
			prev_blk = blk;
			--need;
			if (get_blkinfo_by_blkno(blk, blkinfo) == ERRNO)
			{
				//printf("blk = %ld\n", blk);
				free(blkinfo);
				return ERRNO;
			}
			blkinfo->size = 0;
			memset(blkinfo->data, 0, MAX_DATA_IN_BLOCK);
			//被分配的块发生变化，要写回磁盘
		}
	}
	blkinfo->nNextBlock = -1;
	//printf("prev_blk = %ld\n", prev_blk);
	fseek(fp, FS_BLOCK_SIZE * prev_blk, SEEK_SET);
	fwrite(blkinfo, FS_BLOCK_SIZE, 1, fp);

	fseek(fp, FS_BLOCK_SIZE, SEEK_SET);
	fwrite(bitmap, sizeof(bitmap), 1, fp);    //把更新后的位图写回磁盘
	/*if (fclose(fp) != 0)
	{	
		printf("init_sb_bitmap_data_blocks  error :close newdisk failed !\n");
	}*/
	free(blkinfo);
	return num - need;
}

int isBlkAvailable(long blk, unsigned char* bitmap)
{
	return !(bitmap[blk / 8] & (1 << (blk % 8)));
}

void setBitmap(long blk, unsigned char* bitmap)
{
	bitmap[blk / 8] |= (1 << (blk % 8));
}

int findParentAndChild(const char* path, char* parent_path, char* child_path)    
{
	char* path_cpy = strdup(path), *p = path_cpy, *q;
	while(1)
	{
		q = strchr(p + 1, '/');
		//printf("p = %s, q = %s\n", p, q);
		if (!q)
		{
			if (strcmp(path_cpy, p) == 0)    //其实不会出现这种情况，因为传进来的一定有'/'
			{
				strcpy(parent_path, "root");
				strcpy(child_path, p);
				break;
			}
			else	
			{
				*p = '\0';
				strcpy(parent_path, path_cpy);
				strcpy(child_path, p + 1);
				break;
			}
		}
		p = q;
		//sleep(1);
	}
	//printf("parent_path = %s, child_path = %s\n", parent_path, child_path);
	return 0;
}

int get_blkinfo_by_blkno(long blk, struct u_fs_disk_block* blk_info)    //把blk的内容写入到blk_info当中
{
	fseek(fp, FS_BLOCK_SIZE * blk, SEEK_SET);
	fread(blk_info, sizeof(struct u_fs_disk_block), 1, fp);
	if (ferror(fp) || feof(fp)) {
		return -ERRNO;
	}
	return 0;
}

int init_sb_bitmap_data_blocks()
{
	struct super_block* sb = malloc(sizeof(struct super_block));
		//超级块
	sb->fs_size = ftell(fp) / FS_BLOCK_SIZE;    //块大小
	sb->first_blk = BITMAP_BLOCK + 1;    //根目录块号
	sb->bitmap = BITMAP_BLOCK;

	if (fseek(fp, 0, SEEK_SET) != 0)    //找到0号块
	{
		printf("failed!\n");
	}
	fwrite(sb, sizeof(struct super_block), 1, fp);    
		//把超级块的内容写入0号块
	
	//前面1280个块是位图，都被使用
	unsigned char a[160];    //[00000200 ,000002a0), 对应到块[1, 1.3125)，位图的这段表示块[0, 1280]
	memset(a, 255, 160);
	fseek(fp, FS_BLOCK_SIZE, SEEK_SET);    //这个可能要保留
	fwrite(a, 160, 1, fp);

	
	unsigned char tmp = 3;    //[000002a0, 000002a8), 对应到块[1.3125, ), 位图的这段表示块[1281, 1288] 
	//fseek(fp, 160 + FS_BLOCK_SIZE, SEEK_SET);
	fwrite(&tmp, sizeof(unsigned char), 1, fp);
	
	unsigned char b[1119];    //[000002a8, 00000700), 对应到块[, 3.5), 位图的这段表示块[1288, 10240]
	memset(b, 0, 1119);
	//fseek(fp, 161 + FS_BLOCK_SIZE, SEEK_SET);
	fwrite(b, 1119, 1, fp);
	
	int rest = (BITMAP_BLOCK * FS_BLOCK_SIZE * 8 - TOTAL_BLOCK_NUM) / 8;    //[00000700, 000a0200)，对应到块[3.5, 1280]
	printf("rest = %d\n", rest);
	unsigned char c[rest];
	memset(c, 255, rest);
	//fseek(fp, 1280 + FS_BLOCK_SIZE, SEEK_SET);
	fwrite(c, rest, 1, fp);
	printf("init_bitmap_block_successfully!\n");

	struct u_fs_disk_block* root = malloc(sizeof(struct u_fs_disk_block));
	root->size = 0;
	root->nNextBlock = -1;
	root->data[0] = '\0';

	fseek(fp, FS_BLOCK_SIZE * (BITMAP_BLOCK + SUPER_BLOCK), SEEK_SET);
	fwrite(root, sizeof(struct u_fs_disk_block), 1, fp);
	printf("init_sb_bitmap_data_blocks initial root_directory success!\n");

	fseek(fp, FS_BLOCK_SIZE * (SUPER_BLOCK + BITMAP_BLOCK + ROOT_DIR_BLOCK), SEEK_SET);
	int rest2 = (TOTAL_BLOCK_NUM - (SUPER_BLOCK + BITMAP_BLOCK + ROOT_DIR_BLOCK)) * FS_BLOCK_SIZE;
	unsigned char d[rest2];
	memset(d, 0, rest2);
	fwrite(d, rest2, 1, fp);
	printf("init_sb_bitmap_data_blocks initial all success!\n");


	free(sb);
	free(root);
	return 0;	
}
