# MIT6.828-2017Lab5 学习过程

### 0.开始

本次Lab是最简单的一个。但是，如果像深入了解，可能花费很长时间（硬盘驱动程序，键盘驱动程序，串行驱动程序，微内核style的文件系统，各种设计模式，shell的实现）

实验的所有代码提交可参见[我的GitHub仓库](https://github.com/astzls213/MIT6.828-2017Lab)

我更建议大家看`github`仓库那个`experience`文件夹里头的，因为知乎对md格式排版不是很好。。最后，如果你觉得干货满满，可以点个赞鼓励下我！！嘻嘻！如果有什么不懂的，可以评论留言告诉我。

### 1.为file system进程设置IO特权级

```c
e->env_tf.tf_eflags |= FL_IOPL_3;
```

问题1：

不需要，因为进程间切换时的上下文是在trap那里保存了，然后在env_pop_tf那里恢复，不需要做额外工作。

### 2.实现block cache和write back blcok

JOS采用微内核风格，即文件系统不是由内核实现，而是靠一个特殊进程`fs`服务。

这个进程将会将数据从磁盘读到自己的地址空间中，或者将数据从内存写回磁盘。庆幸的是，我们无需实现磁盘驱动程序，只需利用写好的api调用即可完成任务：

1. `bc_pgfault`当错误地址来自缓冲地址段时，读取对应`block`至该地址。
2. `flush_block`写回地址所处页至磁盘，该页必须是存在且被修改过的，否则不用做任何事。然后再清除dirty位。

实现这两个功能用到了核心函数`ide_read`,`ide_write`。不过要注意的是，这两个函数是磁盘驱动函数，操作单位是扇区（sector），而不是我们的簇（block），在传参时要注意：

```c
ide_read(blockno*BLKSECTS, dst, BLKSECTS);
ide_write(blockno*BLKSECTS, src, BLKSECTS);
```

### 3.实现block分配

`fs`进程还要管控`block`的分配与回收，这就离不开对`block`的使用情况进行记录，记录方式有很多，简单方便高效的方式就是使用`Bitmap`，即每一位对应某一`block`，1为`free`，0为`in-use`。

那么，我们依葫芦画瓢，仿照`free_block`写即可。

### 4.文件数据块的获取

函数解释：

1. `file_block_walk`是用来找第`i`个文件数据块所存储的磁盘`block`是哪个，因为我们采用直接指引和一级间接指引，一级间接指引可能为空指针，所以根据`flag`看看要不要分配一`block`给`indirect`，然后再返回对应的那个`block`号。
2. `file_get_block`是用来获取第`i`个文件数据块所存储的磁盘`block`的`cache`地址。

注意点：

1. `file_block_walk`分配给`indirect`的block必须要清0，因为分配前可能被使用，存在垃圾值。
2. `file_get_block`使用`file_block_walk`获取对应的block number，而`block number`可能为0（清0就是为了识别该数据块未使用的状态），这时我们需要分配一个`block`，并让对应的`slot`指向它。

### 5.实现文件系统的读服务

使用`openfile_lookup`和`file_read`即可，不要忘了修改offset。这里说下定义的结构体：

1. Fsipc是一个联合数据结构，它的大小是一个PGSIZE，所以每个成员的起始地址都是一样的。
2. Fd是File descriptor的简称，用于表明文件类型（文本文件？设备？还是其他什么鬼的），文件当前position。
3. File结构是文件的基本信息，和inode差不多。

### 6.实现文件系统的写服务

`serve_write`没什么好说的，和上面一样。主要说下`devfile_write`。

我先说下用户进程写文件的流程：

1. 调用系统库函数`write`，传入`file descriptor`号。
2. write会找到对应的file `descriptor`，从而知道我们写的对象究竟是文本文件还是设备还是别的类型的东西。`write`就获取到对应的Dev结构体，调用`Dev`里的对应函数成员。（假设我们现在的成员就是`devfile_write`。
3. 现在需要构造请求头`Fsipc`，这个请求头大小为页大小。然后通过调用Fsipc发送请求。
4. Fsipc实际上通过我们上一个实验实现的`ipc_send`发送请求。使用ipc_recv接受回应。

这就是微内核文件系统的架构。也就是说，我们的工作就是构造请求头并使用Fsipc进行发送。注意的是，传给buf的字节不要太多，刚好为请求数量就好。而且请求数量可以是任意的，但是`buf`容量有限，所以当请求数量过大时，就只写`buf`大小的字节数就好了。否则，等待你的就是`Page fault`。

### 7.实现`sys_env_set_trapframe`

这个好简单。。。我都不想说了。。注意要看看tf这个指针指向的是有效内存（使用`user_mem_check`）。

### 8.实现页面共享

就是说，如果权限位有`PTE_SHARE`的，就直接复制映射即可。需要修改duppage和copy_shared_page。如果你会做Lab4 PartC，这里就不难了。

### 9.处理键盘中断和串行中断

就是向trap_dispatch·里增加判断即可，函数人家都写好了，调用即可。

### 10.实现输入重定向

模仿输出重定向的实现即可，依葫芦画瓢。

这样我们就结束了Lab5，作者接下来，就是自己DIY一个OS。期待以后的博客吧。

