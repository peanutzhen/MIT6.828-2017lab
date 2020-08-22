# MIT6.828-2017 Lab3学习过程

### 0.实验简介

> In this lab you will implement the basic kernel facilities required to get a protected user-mode environment (i.e., "process") running. You will enhance the JOS kernel to set up the data structures to keep track of user environments, create a single user environment, load a program image into it, and start it running. You will also make the JOS kernel capable of handling any system calls the user environment makes and handling any other exceptions it causes.

由此可见，实验3的内容就是实现进程管理。

实验的所有代码提交可参见[我的GitHub仓库](https://github.com/astzls213/MIT6.828-2017Lab)

知乎的盆友们可以先收藏起来，全篇5432字，慢慢看，如果点个赞就更棒棒了呢。

### 1.建立环境表

和Lab 2的在`mem_init`建立物理页信息表`pages`一样，我就不多说了。

### 2.初始化环境控制块ECB

1. 首先构成空闲环境控制块链表，为分配一个`ECB`做准备。
2. 实现环境的虚拟地址空间中内核部分的映射。
3. 实现为环境分配内存，并在它的页表完成相应映射。
4. 将环境虚拟地址空间的用户部分填充。
5. 实现`env_create`函数，该函数功能是创建一个环境（具有可执行能力）。
6. 实现`env_run`函数，该函数实现环境的上下文切换。

我个人感觉这里不难，环境和进程的概念是类似的，至少我感觉没什么区别。但是要注意这几个点：

1. 要注意`cr3`这个寄存器当前指向哪个`PD`，如果指向`kern_pgdir`，则不能访问当前环境的用户部分的虚拟地址空间（没有建立映射）。所以要使用`lcr3`函数来更改`PD`指向。
2. 加载`ELF`也不难（如果在LAB 1认真分析了`boot/main.c`的工作流程），就是内存内容的迁移，有手就行。
3. 为环境分配内存，就是计算分配所需页数，使用`page_alloc`分配一页，然后用`page_insert`即可完成对应虚拟地址的映射。
4. 作者不明白为什么`env_run`时，不需要保留待切换环境的上下文？很奇怪。

如果写完后，使用`gdb`调试，能够在`int 0x30`停下，说明你的代码通过了。

### 3.添加相应中断功能

这个练习有点难，后来阅读`xv6 book chapter3/intel manual`才搞懂是怎么回事。

简要说一下这个练习要我们干啥：

1. 为每个在`inc/trap.h`定义的`trap/interrupt`实现对应中断入口。
2. 初始化`trap_init`函数。（为`IDT`对应项填充信息）

那么根据阅读官网的提示，以及`trapentry.S`的注释以及代码。可以发现，中断流程是这样子的：

1. 发现有`interrupt/trap`出现。
2. 获取对应的中断号`n`。
3. 将必要信息压入`stack`中，比如当中断发生需要切换模式时，依次压入：`ss, esp, eflags, cs, eip, error_code`。
4. 将`CS:EIP`修改为`IDT[n]`对应字段。（实现跳转，若这里权限不够，会引发13号中断）
5. 再跳转至`_alltraps`
6. 再跳转至`trap()`（`kern/trap.c`文件中）
7. `trap()`又调用`trap_dispatch()`
8. 再调用`print_trapframe()`

这就是目前中断发生时所做的事情。现在就应该很清楚了，`kern/trapentry.S`定义的`TRAPHANDLER/TRAPHANDLER_NOEC`是用来设置中断入口的。为什么这两个`macro`的最后一条指令是`jmp _alltraps`呢？因为我们发现中断程序前半部分代码是各自独立的，而后半部分（`_alltraps`）是各自相同的（压入一些值，然后跳转）。所以把变的部分抽离出来，不变的就可以实现代码复用，减少编译后代码长度，更加容易维护的优点。

关于**`_alltraps`**如何实现，官网说的已经很清楚了，我就不多说了。现在我们要初始化`IDT`，这样上述所说的流程就能执行了。`mmu.h`定义的`SETGATE`宏能够帮助我们设置对应项的信息，`SETGATE`传入`5`个参数，分别是：`对应项， 中断类型，  GDT索引， 入口函数地址， 权限级别`。由于我们的中断程序是和`kernel`代码一起的，所以`GDT索引`应该是`GD_KT`，即`Kernel_text`（内核代码段），`dpl`有`4`个可选的值 0～3 ，数字越大权限越低，`trap`类型的中断，`dpl`通常为`0`。而`iterrupt`类型的，`dpl`视情况而定。

那么我们回答一下**问题**：

1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

   **exception和interrupt在真正处理中断程序之前，必须构建一个`TrapFrame`结构体，使得随后的中断处理程序能够运行。如果全部`异常/中断`都共用一个`handler`，那么不仅`Trap number`丢失，而且`TrapFrame`也不好构建（有些`int`传`Error_code`，有些不传）**

2. Did you have to do anything to make the `user/softint` program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but `softint`'s code says `int $14`. *Why* should this produce interrupt vector 13? What happens if the kernel actually allows `softint`'s `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

   **不用再做anything。`14`号中断是`Page Fault`，然而，只有当`CPL == 0`才可执行该`int`指令（`Page Fault`的`DPL`是0）。然而这是一个用户程序，是无法执行的。由于如此，执行`int 0xE`时，会产生`13`号中断（`General Protect Fault`）。所以脚本期望我们产生这个中断。如果用户程序可以执行这个中断，那么一个程序可以通过不断发出这个中断信号来不断获取额外的物理页，显然不合理。**

### 4.设置缺页处理程序

在`trap_dispatch()`设置即可，太简单，不多说。可通过`make grade`看下是否通过。

### 5.设置断点处理程序

像设置缺页处理程序那样写就行了，不过这里调用的是`monitor()`。

**回答一下问题：**

1. The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to `SETGATE` from `trap_init`). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?

   如果`SETGATE`时，将`BREAKPOINT`的`dpl`设置成`0`，那么肯定触发 13 号中断，而不是断点中断，因为`make grade`是在`user mode`发出的中断，所以要将`dpl`设置成`3`。

2. What do you think is the point of these mechanisms, particularly in light of what the `user/softint` test program does?

   防止用户程序恶意使用中断。

### 6.设置系统调用中断

也是和前面一样，但我要解释下：

1. `lib/syscall.c`是定义系统调用API，供给用户使用。
2. `kern/syscall.c`是定义对应系统调用的处理程序，供给系统处理使用。

在`trap_dispatch()`调用`syscall`时，参数是存在于`tf->tf_regs`中，因为在调用`int 0x30`前，参数已经在寄存器里，所以`TrapFrame`会保存它们。然后请将`syscall`的返回值写到`tf->tf_regs.reg_eax`中，这样恢复上下文就可以获取到调用后的返回值。

### 7.设置`thisenv`

这个练习，作者遇到一个未解之谜bug。原本这个练习十分简单，只需将`libmain.c`修改成：

```c
thisenv = envs + ENVX(sys_getenvid());
```

就行了。但是！不知道是不是我前面函数写错了还是怎么，这样写会无限生成Page Fault，第一次出现是在`load_icode`的复制用户程序至对应虚拟地址空间的代码那里，然后由于`page_fault_handler`又调用`env_destory`，而它又调用`cprintf`去输出`e->envid`，没想到这里又出现`Page Fault`，然后就无限死循环，直到`stack overflow`。

神奇的事情又出现了，我如果把这段逻辑分成两部分：

```c
// libmain.c
thisenv = envs;
// user/sendpage.c的第一条语句
thisenv += ENVX(sys_getenvid());
```

就可以正常执行，意思就是说修改`thisenv`的正确下标延后至`umain`去执行，而不是`libmain`，我现在都不知道为什么这样可以，而那样不行。。可是我认为我前面写的代码没错啊。。（如果有错，是不可能通过更改一个**与出错部分毫不相干的代码**而修复bug）

不过也有可能是我错了，但我真的不知道为什么。。

### 8.优化`page_fault_handler`

这个练习要求你：

1. 修改`page_fault_handler`，使得如果是内核态页出错，抛出`panic`
2. 修改`user_mem_check`，使`user_mem_assert`可以在`sys_cputs`可以使用。
3. 为`kedebug.c`的`debuginfo_eip`使用`user_mem_check`来检查一些内存是否有效

这个练习唯一注意的就是`user_mem_check_addr`的范围是在va ～ va+len里的，那么也就是说，`va`是要被要求页对齐的，而如果此时`va`出错了，而返回的是对齐后的`va`，显然可能不在上述所说的范围里，所以要返回未对齐的。而之后是一定在这个范围的，所以直接返回对齐的`va`。

至于那个问题，我不知道为什么。。