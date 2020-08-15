# MIT6.828-2017Lab4PartA 学习过程

### 0.开始

本次实验，要我们实现多处理机的初始化，以及多任务的运行机制，并实现全抢占式的循环调度。

实验的所有代码提交可参见[我的GitHub仓库](https://github.com/astzls213/MIT6.828-2017Lab)

由于觉得之前博客有点啰嗦，这次使用精简文字叙述。

### 1.实现`mmio_map_region` + `page_init`微调

官网说把练习1，2看成一个整体，那么这个练习要求我们做的事就是标题所述。给大家点清几个地方：

1. 可使用`boot_map_region`实现练习1，`size`如果大于`map_region`要`panic`，`size`要`ROUNDUP`。
2. `page_init`只需调整对应`pages`项的指针指向即可（注意链表一开始是怎样的顺序连接的）

**Question**

1. 为什么`kern/mpentry.S`中的符号需要用`MPBOOTPHYS`来重定位？

   因为`mpentry.S`是通过`mpentry_start`来链接的，所以所有符号的地址基于此产生。但`mpentry.S`被加载至物理地址的`0x7000`处（大小为4KB）。所以需要这个宏来使用符号。（JOS的段机制没有实现，所以，实模式下的虚拟地址就是物理地址。。）

### 2.设置每个cpu的内核堆栈

使用`boot_map_region`映射。如图所示：

（图以后补充，总的来说就是va：`[KSTACK_i,KSTACKTOP_i]` 映射至pa:`[percpu_kstacks[i],percpu_kstacks[i] + KSTKSIZE]`）

### 3.设置CPU的特殊寄存器值

1. 设置内核栈指针`ss`/`esp`（在`Taskstate`结构体保存）
2. 设置CPU的`TSS`的初始地址在`gdt`保存
3. Use `ltr&lidt` 设置TSS选择子寄存器和中断向量表寄存器

`Selector`/`Descriptor`参考之前的学习记录，我的Github仓库中`sprint_konwledge`有。这里要注意的是：

1. `trap_init_percpu`是每个CPU初始化时调用的，我一开始理解为要在此函数实现所有的CPU初始化。。。所以只需将`ts`替换成`thiscpu->cpu_ts`即可。

### 4.设置锁

这个好简单，不需要你实现锁，跟着官网要求加就完事了。

### 5.实现RR调度

步骤：

1. 实现`sched_yield()`，实现RR调度。
2. 修改`kern/syscall.c`，实现调度的系统调用处理。
3. 将`kern/init.c`的`i386_init`最后的`env_create(user_primes)`，改为创造3个以上的`user_yield`进程。

注意点：

1. JOS 没有实现跟踪已分配的进程控制块的表，所以我们必须从上一次运行进程的位置开始遍历`envs`。

2. 注意一些极端情形：

   （1）上一次运行进程是NULL，所以要从`envs`起始开始遍历。

   （2）如果遍历完都没有可运行的进程，那就继续运行上一次的进程（如果可以的话）

3. `sched_yield`不需要设置上一次进程为`RUNNABLE`，这个工作是`env_run`所做的。

有意思的是，我们还没有实现时间中断事件，所以是进程主动提出调度的，而不是按照时间片调度的。

回答一下问题：

1. 为什么`env_run`的参数`e`可以解引用在两个页表之间？

   因为`e`是在`envs`那段虚拟内存里的，那个是所有进程都会被映射的。所以可以用。

2. 上一个进程的上下文在那里被保存？

   在`trap()`那里保存起来了。（Lab3我还很奇怪来着）

### 6.实现进程的无性繁殖（fork）

`dumbfork.c`父进程调用流程：

1. `dumbfork()`

   1. `sys_exofork()`

      这里，创建新的PCB（即Env结构体），使用`env_alloc()`。然后只需修改一下该PCB的`status = ENV_NOT_RUNNABLE`（防止被调度，现在还没准备好） and `env_tf = thisenv->env_tf`，并且修改eax为0（这样等该进程可以执行时，`sys_exofork()`返回值就为0.）

   2. `duppage()`

      1. `sys_page_alloc()`

         为子进程分配一页物理页

      2. `sys_page_map()`

         将父进程的UTEMP映射子进程的某处。这样父进程，就能访问子进程的虚拟地址空间（仅一页大小）

      3. `memmove()`

         将**当前虚拟地址空间**的一处内容复制到另一处！注意是当前地址空间！现在执行的是父进程，所以`cr3`指向的是父进程的页表。现在知道为什么要进行`page_map()`了吧。

      4. `sys_page_unmap()`

         取消父进程对子进程对映射。

   3. `sys_env_set_status()`

      设置子进程可以允运行了（现在子进程的虚拟地址空间几乎和父进程一毛一样，除了那个eax）

2. `cprintf()`

   屏幕输出字符串。

所以，现在各位应该很明确了吧，代码什么的，按照注释提示写就好了，直接告诉你怎么写就没意思了。实在不会可以看我的仓库。

运行能出现那些字符串然后返回到JOS monitor就算通过了！Part A结束。

