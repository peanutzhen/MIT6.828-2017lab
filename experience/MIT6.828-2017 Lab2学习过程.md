# MIT6.828-2017 Lab2学习过程

### 0.实验简介

> In this lab, you will write the memory management code for your operating system. Memory management has two components.

由此可见，实验2的内容就是实现内存管理（内存分配，内存映射）

实验的所有代码提交可参见[我的GitHub仓库](https://github.com/astzls213/MIT6.828-2017Lab)

知乎的盆友们可以先收藏起来，全篇5500字，慢慢看，如果点个赞就更棒棒了呢。

### 1.物理页管理（内存分配）

回想一下，在 Lab1 时，我们就已经完成了一个简单的内存映射：**将虚拟地址`[0, 4MB]`映射到物理地址的`[0, 4MB]`;将虚拟地址`[0xf0000000, 0xf0400000]`映射到物理地址`[0, 4MB]`。**这个映射是通过`cr3`记录下`entrypgdir.c`中的`entrypgdir`数组的首地址完成的。

练习1要求我们实现，物理页面的管理。还是一样，作者从`init.c`的指令调用顺序来阅读代码，发现调用流程为`mem_init`->`boot_alloc`->`page_init`。

通过阅读`boot_alloc`，得知`nextfree`是用来指向一片空闲区的首地址指针，它被初始化在`bss`段（该段后无更多段，说明后面的内存是空闲的）的末尾（且对齐（ROUNDUP宏））。`result`就是用来返回值的。那么参数`n`代表申请分配 n 个字节。所以，`n > 0`时，我们只需将`nextfree += n`即可，如果`nextfree`大于`0xf0400000`则不能分配，应抛出一个`panic`。因为我们只映射了前4MB内存，如果方位超过4MB则会出错。用`result`先存下一开始的`nextfree`作为返回值，并将更新后的`nextfree`对齐。

然后，要求我们实现使用`boot_alloc`为`pages`划分出足够的空间，这个很简单，就像你平时用`malloc`那样即可。

接着来到`page_init`，`JOS Kernel`将用一个线性数组`pages`记录下所有物理页的信息。这里他直接将所有物理页都设置为空闲，这显然是不对的。所以我们要求实现正确的`pa ge_free_list`。`page_free_list`就是一个指向链表的指针，每一块空闲的物理页会指向下一个空闲的物理页。

那么，`page_alloc`和`page_free`就不难实现了，就是链表的简单操作罢了。

### 2.虚拟页管理（内存映射）

虚拟页的管理就是将**虚拟页**映射到**物理页**。因此，为了实现这种**映射**关系，我们这个练习的重点就是关注**二级页表**。二级页表由**Page Directory**/**Page Table**两部分组成。PD我们已经使用`boot_alloc`在`mem_init`里分配了，但是PT还是未分配的（我们只是分配了所有物理页信息的空间，由`pages`指向，用于物理页分配）。所以我们可以用`page_alloc`来申请一块物理页，作为一个PDE项（即Page Table地址）。

所谓**映射**，就是靠`PTE`的地址段实现，而为了找到`PTE`，要通过`va`（虚拟地址）。

`pgdir_walk`函数要求实现：给定一个**虚拟地址（线性地址）va**，返回对应的**页表项PTE**。       `boot_map_region`：将一段**虚拟地址空间**映射到**物理地址空间**上。
`page_lookup`：给定**虚拟地址**，查询它映射到的物理页信息。
`page_remove`：给定**虚拟地址**，将其映射抹去。
`page_insert`：给定一个**虚拟地址**和**物理页信息**，建立两者的**映射**。

搞清楚这些函数要实现的功能后，再去写代码相信就不难了（结合`mmu.h`,`pmap.h`所提供的函数和宏）。但是有几个注意的点：

1. `page_insert`的注释里说用一种巧妙的方式来实现`corner-case`（即重复将`va`映射到同一张物理页`pp`上去），而**不必使用额外的代码去实现它**。作者一开始先不管这个特殊情况，先写代码，看他怎么报错。然后，发现重复映射时，我的代码先将旧映射给抹去（调用`page_remove`)，然后在重新写`pte_t`相关信息，重新映射，结果出错了。原来，这样在重复映射同一张物理页时，会把本来是占用的物理页变成**空闲**的物理页，而要想他不变成空闲（即在`page_remove`里不调用`page_free`），就要先将`pp_ref`成员先递增，而不是后递增（这样的话，就会先`free`掉了）。到这里我才晓得，为什么不比在写一段`if`来判断了，类目。
2. `pgdir_walk`返回的`pte_t *`必须是**虚拟地址**！作者一开始直接就返回它的物理地址，结果跑的时候，直接出错，就像 Lab1 将链接地址修改成其他地址时那样的错误。于是作者gdb疯狂调试，发现返回的`pte_t *`是`0x3ff000`，也就是说，是在最开始的`4mb`内存空间里的。然而，我们正处于保护模式，也就是说，`0x3ff000`这个地址虽然我们期望它是**物理地址**，但计算机是当它为**逻辑地址（虚拟地址）**的，因此，在 Lab1 当中，我们设置了一张 GDT ，并将所有的段基址设置成0，那么在 JOS 里，`segment translation`实际上没将虚拟地址改变，原样输出。那么现在地址变成**线性地址**，经过`Page Table`转换，得到的地址才为**物理地址**。但是，我们不是已经在`entry.S`将**线性地址空间`[0,4MB]`映射到物理地址`[0,4MB]`**了吗？现在访问怎么会出错呢？后来阅读`entry.S`，发现说这个映射很快就会被撤除（在`entry.S`进入`i386_init`后撤除）。怪不得这里会报错，所以，将`pgdir_walk`返回的地址改成**虚拟地址**即可。

### 3.虚拟地址空间映射

这个练习要我们将虚拟地址空间的某些部分，映射到相应的物理地址空间上去：

1. 将`UPAGES`映射页面信息`pages`所在的物理地址空间。
2. 将`KSTACKTOP`映射到`bootstacktop`所在地物理地址空间。
3. 将`KERNBASE`映射到`0x0`起始的物理地址空间。

所以，我们必须搞清楚映射的**起始虚拟地址**，**映射范围**，**起始物理地址**以及**权限**。然后调用`boot_map_region`即可完成映射。这些信息均可以从原注释，以及`memlayout.h`找到。这样我们就完成了所有的基础练习。

接下来，回答一下**问题**：

1. What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

   | Entry | Base Virtual Address | Points to (logically):                |
   | ----- | -------------------- | ------------------------------------- |
   | 1023  | 0xFFC00000           | Page table for top 4MB of phys memory |
   | 1022  | 0xFF800000           | Page table（映射至物理地址次高的4MB） |
   | .     | ?                    | ?                                     |
   | .     | ?                    | ?                                     |
   | .     | ?                    | ?                                     |
   | 2     | 0x00800000           | Page table（映射至程序data&heap）     |
   | 1     | 0x00400000           | NULL（未映射）                        |
   | 0     | 0x00000000           | NULL（未映射）                        |

2. We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

   **很简单，如果用户程序强行访问高地址的内核部分，很有可能造成内核代码，数据，栈道破坏，这是灾难性的。所以我们可以设置`PTE`对应的权限位来禁止访问。**

3. What is the maximum amount of physical memory that this operating system can support? Why?

   **内核支持最大物理内存由`Page Table`来决定。在`JOS`中，运用了二级页表，`PDE/PTE`大小均为一页面，那么`PTE`数量总共为`1024*1024`，而每个`PTE`指向一个大小为`4KB`的页面。所以最大物理内存支持为`4GB`。**

4. How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

   **当完全映射`4GB`时，`Page Table`占用`(1024 + 1) * 4KB`，即大约为`4MB`的内存。然后`pages`也要占用`10MB`来存储每个物理页信息。想要进一步减少内存占用，可以将`pages`这个链表，不要以每个空闲页作为单位，用空闲内存块作为单位，就可减少链表长度。`Page Table`可以在增加至三级。**

5. Revisit the page table setup in `kern/entry.S` and `kern/entrypgdir.c`. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

   **在`ljmp`后`EIP`才在`KERNBASE`后；因为`entry_pgdir`也映射了虚拟地址的低 4MB 到物理地址的低 4MB。如果不映射，那就会无法执行了。（我之前讲过）**

   

### 4.总结

挑战练习我就不做了，等做完`6.828`，对内核有比较深刻的理解后，再在我的内核上写。

通过本次实验，攻略在校学的内存映射部分的知识，对内存分配，内存映射有了更加具象的认识。对保护模式下，地址转换有了更加清晰的认识，但对段转换的具体细节还不是很清楚，需要继续往后做实验深入。

但是我对本次实验的内存管理实现感到不满，JOS将空闲内存做成了一个空闲页链表。当机器内存很大时，这个链表就会很长。而且到时为进程分配空间时，要连续分配好多张页，这里的时间复杂度应该怎么消除呢？不知道，继续往后做实验吧。

### 2020.7.31 updated

通过稍微阅读了`intel `官方手册`，知道了段转换的细节了：

1. 段寄存器用于保存`Selector`的值（`Selector`格式见`spring_knowledges/lab2`）
2. 用`Selector`定位`GDT`对应项，就可以知道所访问段的`Base Addr`,`Limit`,`各种权限`
3. 通过`Baseaddr + 基址寄存器值`得到所谓的`Linear address`。

那么，`GDT`就是我们国内学校所学的`段表`，原来只是换了个说法，害得我迷惑好久，大无语。