# 实现了个残废文件系统
## 0 写在前面 
事先说明，虽然说这次的课设要求是实现fuse提供的接口，但是至少出现在这里的内容，和fuse没有任何关系，所以想要一字不动地拿过去当成课设最后成果的，就用不着往下看了。

还有就是，感谢前人做出来的一些成果，虽然说可能他们也是借鉴别人的，毕竟这个课设，十几年都没变过，但不管怎么说，有些东西虽然很旧了，但还是值得参考的。当然也可能存在这样的情况：有些谬误，或者说糟粕，代代相传，最后成为“屎山”。下面是值得参考的一些内容，这次的一些思想也是借鉴这里的：

[https://blog.csdn.net/u012587561/article/details/50908656](https://blog.csdn.net/u012587561/article/details/50908656)

## 1 模拟磁盘文件

### 1.1 创建磁盘文件
文件是放在磁盘上的，而用户态是没有办法直接访问磁盘的。因此想要直观地观察到磁盘内的信息，并不是一件容易的事。好在，我们可以用一个文件模拟磁盘，然后再这个文件上进行文件读写和创建等文件系统的基本操作。这个磁盘文件，是可以在用户态下访问的。

怎样去创建并初始化这个磁盘文件呢？首先要明确磁盘文件的大小和块大小，这次我们需要5M的磁盘，块大小是512，有5M / 512 = 10240个块，因此切换到相应的工作目录后，通过命令dd创建文件：

```bash
 dd if=/dev/zero of=diskimg bs=512 count=10240
```

这样之后就创建了符合要求的磁盘文件。

### 1.2 初始化磁盘文件
接下来就要考虑如何初始化了，因为磁盘文件一开始是全零的，但是这并不是我们想要看到的情况。有哪些块需要处理呢？在这个文件系统下，磁盘的0号块是超级块，1号到1280号块是位图，用来标记每个块是否被使用，1281号块是根目录块。也就是说，这些块是需要被初始化处理的。

怎么去访问到这些块呢？我们通过一个函数init_sb_bitmap_data_blocks()来实现。在此之前，先看一下整个项目的调用流程。

#### 1.2.1 执行流程
在终端输入dd命令后，编译并执行文件：

```bash
gcc utils.c utils_main.c -o utils
./utils
```

#### 1.2.2 初始化函数init_sb_bitmap_data_blocks
想要初始化磁盘，输入init即可。init函数并不难写，先看超级块，结构体super_block定义在global.h：

```c
struct super_block 
{ 
	long fs_size;    //整个文件系统共有多少块
	long first_blk;    //根目录块的块号
	long bitmap;    //用作位图的块的数量
};
```

##### 1.2.2.1 初始化超级块
下面是函数init_sb_bitmap_data_blocks对超级块，即0号块的初始化：

```c
struct super_block* sb = malloc(sizeof(struct super_block));
		//超级块
sb->fs_size = ftell(fp) / FS_BLOCK_SIZE;    //块数量
sb->first_blk = BITMAP_BLOCK + 1;    //根目录块号
sb->bitmap = BITMAP_BLOCK;    //用作位图的块的数量

if (fseek(fp, 0, SEEK_SET) != 0)    //找到0号块
{
	printf("failed!\n");
}
fwrite(sb, sizeof(struct super_block), 1, fp);    
	//把超级块的内容写入0号块
```

##### 1.2.2.2 初始化位图
超级块存了一些整个文件系统的信息，因此需要在使用前初始化。接下来是初始化位图块。值得注意的是，用作位图的块是1号块到1280号块，共1280个块用作位图。而事实上这么多块是远远超出使用需要的。文件系统有10240块，需要10240个位来表示是否使用，而每个块大小是512KB，也就是4096位，因此只需要10240 / 4096 = 2.5个块，剩下的块是没有表示作用的。初始化位图块的代码如下：

```c
unsigned char a[160];    
	//[00000200 ,000002a0), 对应到块[1, 1.3125)，位图的这段表示块[0, 1279]
memset(a, 255, 160);    //全部位为1，共160 * 8 = 1280位
fseek(fp, FS_BLOCK_SIZE, SEEK_SET);    //定位到1号块
fwrite(a, 160, 1, fp);    //写入磁盘


unsigned char tmp = 3;    
	//[000002a0, 000002a8), 对应到块[1.3125, ), 位图的这段表示块[1280, 1287]    
fwrite(&tmp, sizeof(unsigned char), 1, fp);
	/*块1280（用作位图的最后一块）和块1281都（根目录块）被使用，因此位图对
	应的这两位也置为1*/

unsigned char b[1119];    
	//[000002a8, 00000700), 对应到块[, 3.5), 位图的这段表示块[1288, 10240]
memset(b, 0, 1119);    //全部置0，因为后面的块都未使用
//fseek(fp, 161 + FS_BLOCK_SIZE, SEEK_SET);
fwrite(b, 1119, 1, fp);

int rest = (BITMAP_BLOCK * FS_BLOCK_SIZE * 8 - TOTAL_BLOCK_NUM) / 8; 
	//[00000700, 000a0200)，对应到块[3.5, 1280]
//printf("rest = %d\n", rest);
unsigned char c[rest];    
memset(c, 255, rest);	//位图的这部分不表示任何块，因此全部置1
//fseek(fp, 1280 + FS_BLOCK_SIZE, SEEK_SET);
fwrite(c, rest, 1, fp);    //写入磁盘，
printf("init_bitmap_block_successfully!\n");    //写完位图
```

##### 1.2.2.3 初始化剩下的所有块
剩下的块全部置0就可以了：

```c
fseek(fp, FS_BLOCK_SIZE * (SUPER_BLOCK + BITMAP_BLOCK + ROOT_DIR_BLOCK), SEEK_SET);
int rest2 = (TOTAL_BLOCK_NUM - (SUPER_BLOCK + BITMAP_BLOCK + ROOT_DIR_BLOCK)) * FS_BLOCK_SIZE;
unsigned char d[rest2];
memset(d, 0, rest2);
fwrite(d, rest2, 1, fp);
printf("init_sb_bitmap_data_blocks initial all success!\n");


free(sb);
free(root);
return 0;	
```

## 2 目录项struct u_fs_file_directory
目录项是一个很重要的结构体，定义在global.h：

```c
struct u_fs_file_directory { //64 bytes
	char fname[MAX_FILENAME + 1];    //目录名或文件名
	char fext[MAX_EXTENSION + 1];    //扩展名(如.txt)
	long fsize;    //该目录或文件的大小
	long nStartBlock;    //该目录或文件的起始块
	int flag;    //1表示文件，2表示目录
};
```

为什么说目录项很重要呢？目录项让我们可以根据文件或目录的绝对路径，找到这个文件或目录在磁盘上的位置。想象一下，现在我们有一个文件的绝对路径aaa/bbb/ccc.c，我们怎么样找到这个文件在磁盘上的位置呢？

### 2.1 如何根据目录项找到文件或目录在磁盘上的位置？

块的定义在global.h：

```c
struct u_fs_disk_block { // 512 bytes
	int size;    //该块data被使用的大小
	long nNextBlock;    //下一个块的块号
	char data[MAX_DATA_IN_BLOCK];    //存数据
};
```

首先，我们从根目录块开始，读取data（根目录块以及所有后续目录块存的是一条一条的目录项struct u_fs_file_directory）data，每次偏移sizeof(struct u_fs_file_directory)的大小，若没有找到fname == "aaa"的目录项，就说明不存在绝对路径为aaa/bbb/ccc.c的目录或文件；若找到了，就根据目录项的nStartBlock，找到目录“aaa”起始块，然后用同样的方法去查找"bbb"的目录项，再找"ccc.c"的目录项，最后定位到aaa/bbb/ccc.c所在的块。

也就是说，**目录项不仅仅是目录才有的，而是每个目录或者文件都有的。**

### 2.2 创建文件或目录utils_create

搞清楚了目录项以后，就可以开始想怎么样去创建一个文件或目录了。其实这两者本质上没有什么区别，而且做法大致相同，因此可以用同一个函数来实现。

先来看一下utils_create的声明：

```c
int utils_create(const char* path, int flag);
```

#### 2.2.1 utils_create的参数
flag决定了这次创建的是目录还是文件，1是文件，2是目录，其余则直接返回-1，什么也不做。那么path是怎么回事呢？假设现在path == “aaa/bbb/ccc.c", flag == 1，那这个函数要做的事情就是在目录"aaa/bbb"下创建文件ccc.c **（这个文件系统不考虑相对路径，只考虑绝对路径）**。当然，如果path是"aaa"这种，就是直接在根目录下创建目录"aaa"。

#### 2.2.2 伪代码
因此，想要实现utils_create预期的功能，需要做到：

```c
int utils_create(const char* path, int flag)
{
	if (根目录或后续块已经存在path的目录项)
	{
		return 0;
	}
	if (path无'/')
	{
		创建path的目录项;
		将该目录项写回根目录块或后续块;
		return 0;
	}
	找到最后一个'/'，分离出两边，父路径和子路径;
	if (父路径的目录项不存在)
	{
		return -1;
	}
	创建子路径的目录项;
	根据父路径的目录项，在该路径的起始块nStartBlock或起始块的后续块
		nNextBlock下写入子路径的目录项;
	return 0;
}
```

以上过程实现起来并不算很复杂，只是比较繁琐，很容易出现思维漏洞。这其中的关键就是根据绝对路径得到目录项的函数get_dir_by_path；还有就是，新建的目录项，应该写入到磁盘的那个块？可能现有的块的剩余空间不足以写入这个新的目录项，这个时候就要分配一个新块，于是才会有alloc_blocks()；还有一个问题，新的目录项也有起始块nStartBlock，这个起始块一开始是置为-1表示不存在，等到这个目录项代表的目录下有了子目录或文件再分配新块呢，还是在目录项被创建的同时就分配块作为起始块呢？这些都是需要去考虑的问题。

## 3 其它函数
还有很多功能可以去实现，这次除了create，还实现了write，还有read和readdir也是很基本的功能，不过目前没有去实现。**write需要考虑的问题就是，如果写的内容超出了当前文件的大小，可能需要分配新的块。** 这些代码的注释都很详细了，这次就先不分析了。

## 4 部分运行结果
编译，运行，根据提示输入相应的指令，结果如下：
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200218161305309.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80MzU5MDYxMQ==,size_16,color_FFFFFF,t_70)
如果想查看磁盘文件的内容，可以输入命令hexdump，这是一个比较实用的命令，如果输入的参数得当的话，是可以比较方便地查看文件的内容的。给大家演示一下，经过刚才的操作后，磁盘文件的内容如下：
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200218161529943.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80MzU5MDYxMQ==,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20200218161556597.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80MzU5MDYxMQ==,size_16,color_FFFFFF,t_70)

## 5 写在最后
第一次写这么大的代码量，之前虽然也接触过一些代码量更大的东西，但是都是以读别人的代码为主，有时候会补充别人写的。而这次是在参考别人的基础上，自己从头写起。其实这还是一个残次品，因为时间关系（当然也是因为有点厌倦，不想在这里花太多时间了），还有很多很基本的功能没有实现，而且还有很多bug（没多久才发现文件后缀没考虑进去）。所以写这篇东西的时候也有点犹豫，甚至有点不想再看自己写的这份代码，有一种“又造了一个学术垃圾”的感觉。不过后来想想，既然写了，就算很不完善，也还是整理一下吧。

更多精彩内容，敬请期待。

2020.2.18



