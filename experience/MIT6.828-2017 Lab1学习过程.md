# MIT6.828-2017 Lab1学习过程

### 0.实验简介

> This lab is split into three parts. The first part concentrates on getting familiarized with x86 assembly language, the QEMU x86 emulator, and the PC's power-on bootstrap procedure. The second part examines the boot loader for our 6.828 kernel, which resides in the boot directory of the lab tree. Finally, the third part delves into the initial template for our 6.828 kernel itself, named JOS, which resides in the kernel directory.

先给大家打预防针，不要以为第一个实验就很简单，实际上，很难（太底层，low穿地心）。当然，大佬当我没说，在下只是刚学完学校os课程的大二学生。所以作者做这个实验，包括写文档，学习知识，都用了3～4天。

本文章很长，可以先收藏起来，一点一点看，不着急。另外，本篇文章可在我的 GitHub 上找到 [MIT6.828-2017Lab/experience](https://github.com/astzls213/MIT6.828-2017lab/tree/master/experience)

### 1.准备实验内核-JOS

```shell
git clone https://pdos.csail.mit.edu/6.828/2017/jos.git mit6828lab	# Download JOS kernel
cd mit6828lab
make					# compile...
make qemu-nox	# Also can: make qemu. nox means without the virtual VGA(读者可以自己试一下区别)
# 推荐使用nox后缀，减少cpu负载量（如果你在Mac使用PD作为虚拟机软件，必须设置其硬件使用率为低，不然开多个虚拟机CPU会发热很严重）
```

**将内核clone到mit6828lab文件夹（名字可以随便取，大家喜欢就好），如果大家想要和我一样用github记录自己的学习过程，可以在github新建一个仓库，并在这个文件夹添加一个remote指向你的仓库，现在可以新建一个分支`master`，以后的`labN`分支可以看作`dev`分支看待（就是做实验的时候在对应的`labN` `commit`，之后在`checkout`到`master`进行`merge`，再`push master`到你的`remote`**

执行完上述命令后，就已经进入JOS的交互界面，输入`kerninfo`可以显示内存信息。

可以通过先按下`crtl+a`，再按下`x`来退出交互界面。

### 2.探索ROM BIOS启动过程

打开两个终端，都cd到lab目录下。分别输入：

```shell
make qemu-nox-gdb		# on terminal 1, first
make gdb						# on terminal 2
```

然后就可以使用gdb进行调试了，不熟悉gdb的知友们可以参考[6.828 Tools Guide](https://pdos.csail.mit.edu/6.828/2017/labguide.html#gdb)（十分简单，作者没用过看一下就懂了）

作者稍微si了几下，发现BIOS首先设置一个stack空间，stack的底部在0x7000处。然后变转去0xfd15c处执行，这里从0x71读信息，再去set 0x92端口的第二位为1（从right开始，1开始计数），然后lidt和lgdt（不知道是什么），然后从实模式切换到保护模式。

官网上描述，接下来就是初始化VGA等设备和中断描述表。紧接着去可挂载设备将boot loader加载到内存。

### 3.Boot Loader揭秘

设置breakpoint在0x7c00，跟踪了一下，这是我的体会。

BIOS将boot loader加载到内存的0x7c00的地方，然后跳转到那里执行bootloader。

首先boot loader的代码由两部分组成：`boot.s`和`main.c`。

boot.s首先关中断，初始化数据段的段地址为0后，启用地址线的第20根的功能（通过写0xd1至0x64端口和写0xdf至0x60端口）。现在的任务是从实模式转到32位保护模式，切换完了再次初始化数据段，以及初始化stack，esp指向boot loader的起始address（0x7c00），现在执行main.c的`bootmain`函数。

bootmain函数在main.c中，它的工序是先将内核ELF头（一个数据结构，包含可执行可链接文件的各种信息）读到0x10000处，然后判断是否为正确的ELF格式，是的话，继续将内核的可执行可链接文件从磁盘读到内存的0x100000处。读完了以后，在entry指向处执行内核的第一个代码。

为了帮助你更好的理解main.c发生了什么，可以参考我仓库里的[main.c](https://github.com/astzls213/MIT6.828-2017lab/blob/master/boot/main.c)。我为每一条代码都写了为什么要这么做的原因。当然可能有误，还请独立思考。

回答一下练习三的问题：

1. 处理器从什么时候开始执行32位代码？究竟是什么原因导致从16位模式切换到32位模式？

   ###### 从一个跳转指令开始，跳转到32位汇编指令。因为cr0这个控制寄存器的最低位置1.

2. 引导加载程序执行 的最后一条指令是什么，刚加载的内核的第一条指令是什么？

   ###### boot loader的最后一条指令是0x7d6b:	call   *0x10018（跳转到内核）

   ###### 0x10000c:	movw   $0x1234,0x472（这是内核第一条指令）

3. 内核的第一条指令在哪里？

   ###### 在0x10000c处

4. 引导加载程序如何确定必须读取多少个扇区才能从磁盘获取整个内核？它在哪里找到此信息？

   ###### 从读出内核的elf头的e_phnum成员来确定读取多少个程序段，而每个proghdr的memsz指明读多少bytes，那么就能确定需要读多少sector。

### 4.指针理解测试

这是本实验最简单的部分，跳过。

### 5.链接

由于这里涉及了作者的知识盲区，作者赶紧抱着CSAPP（chapter 7）啃了起来，稍微看了一下对链接有了一定的认识，接着做实验。

首先讲一下我对链接的看法，链接要做的事就是将目标文件里的符号给分析一下，再将分析后的符号，全部重定位成内存地址。之所以可执行文件可执行，是因为他从一个ascii组成的c代码，转成汇编代码，再转成机器代码，而机器代码操纵的对象只有两个：**寄存器和内存单元**。那么，链接这一任务，可发生在编译后，也可以发生在程序装入内存后，甚至可以发生在执行时（动态链接）。动态链接我就不说了，因为我们的kernel不是动态链接的显然。我们的kernel应该是静态链接，就是发生在编译后的链接。也就是说，此时的kernel的ELF文件，已经是完全链接的了（所有符号有他自己的内存地址指向）。

作者修改了`-Ttext 0x7C00`为`-Ttext 0x0`,之后objdump boot.out。发现VMA和LMA的值确实是0x0，于是乎开始gdb调试，设置breakpoint在`0x7C00`，果然，依旧是boot loader的code，但是`si`了几步后，发现到达`ljmp $0x8, $0x32`时，执行不下去了。原因很简单，因为我们告诉`Makefrag`要将`boot loader`加载到`0x0`处，但实际上，BIOS是将第一个sector（boot loader）加载到`0x7c00`。那么执行时，由于已经是编译且链接过的了文件，里面的符号全部变成内存地址，而这个地址就是根据`0x0`这个起始地址（因为你告诉链接器，你要在`0x0`load 这段程序）生成，那么现在又处于保护模式，访问一个错误的地址段，当然会出错，不让你跳转。

### 6.0x100000的变化

这个exercise要我们调试观察0x00100000开始的8个word（word=2 bytes），分别在BIOS进入Boot loader时，和Boot loader跳转至kernel时，那么我们就来看一下（其实也没什么好看的，0x00100000时kernel驻留地址）

可以发现，刚进入boot loader时，全为0；进入kernel后，就是kernel的对应可执行文件的数据啊。

### 7.内存映射

这个也简单，google一下了解cr0和cr3各个位的功能就好。练习要我们看看，没开启cr0的最高位之前，`0xf0100000`的内容，再单步执行开启页模式后，`0xf0100000`的内容可以发现已经被映射到`0x00100000`了。如果注释掉在`entry.S`的这段代码`movl %eax, %cr0`，必然会引发错误，因`kernel`生成elf文件时，重定位是根据`0xf0100000`这个来生成的。

### 8.理解`kern/printf.c`,`lib/printfmt.c`, and `kern/console.c`; 并实现输出八进制功能

首先我猜`printfmt.c`要放在`lib`，是因为这个模块的函数不仅`kernel`需要，`user program`也同样需要。所以这个模块，实现的是通用的格式化字符串输出逻辑。

`console.c`是实现底层设备的读入/写出的逻辑，所以涉及到许多硬件编程知识（`cga`,`vga`,`serial port`,'`keyboard`)

`printf.c`就是`kernel`专门的字符输出到`console`的逻辑，实际上，它调用了`console.c`的·`cputchar`函数以及`printfmt.c`的`vprintfmt`函数。这其实就是代码复用，很好的设计模式。

实现8进制输出也很简单，通过`getint` and  设置`base`为8并调用`printnum`即可。

我对[printfmt.c]()作了些中文注释，能够帮你更加快速地理解。

回答一下官网上的问题：

1. 解释一下 `printf.c` and `console.c`. 尤其是 `console.c` 提供了什么 api?  `printf.c`是如何使用的?

   ###### 上面说了。

2. 解释一下 line 196 至 206行在console.c

   ###### 我已经将解释写在[console.c](https://github.com/astzls213/MIT6.828-2017lab/blob/master/kern/console.c)

3. 单步调试下列指令：

   ```
   int x = 1, y = 3, z = 4;
   cprintf("x %d, y %x, z %d\n", x, y, z);
   ```

   - In the call to `cprintf()`, to what does `fmt` point? To what does `ap` point?

     ###### fmt指向"x %d, y %x, z %d\n"，ap指向栈中x的地址。

   - List (in order of execution) each call to `cons_putc`, `va_arg`, and `vcprintf`. For `cons_putc`, list its argument as well. For `va_arg`, list what `ap` points to before and after the call. For `vcprintf` list the values of its two arguments.

     ###### `vcprintf`的两个参数和上面一样。调试的时候`cons_putc`的参数用了 eax 寄存器存储，值就是 'x' 。`va_arg`执行前 ap 指向 x，指向后指向 'y'.（指针+4）但是我就不把每次执行的参数情况列出来了，因为这是显然的，没什么好列的。

4. Run the following code.

   ```
   unsigned int i = 0x00646c72;
   cprintf("H%x Wo%s", 57616, &i);
   ```

   What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise.

   ###### 输出是He110 World。其实不用调试，57616 的十六进制就是 e110, %s 是不断输出地址参数的字符，输出完一个字符之后地址++，直至字符是 '\0'。因为x86是小端法，所以`0x00646c72`其实就是对应 ascii 码的`'\0','d','l','r'`。如果不了解小端法，肯定会觉得这样表示很奇怪，通俗地讲，little-endian就是逆序表示法。为什么用这样的方法？作者猜可能跟硬件电路有关系，或者是一种习惯。

   The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set `i` to in order to yield the same output? Would you need to change `57616` to a different value?

   ###### 如果是大端法，只需`i = 0x726c6400`。不用改变 57616 的值，因为输出数字时，不管你是big-endian还是little，其实都是靠除模来获取数字对应进制的每一位进行有序输出。也就是说，数字输出与内存单元表示法无关。

5. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?

   ```
   cprintf("x=%d y=%d", 3);
   ```

   ###### 输出是x=3 y=1604。其实很简单，就是访问了内存不该访问的位置。这里，明显是访问了参数 3 在栈中位置的下一个4字节内存。而这个内存，实际上是`memset`的第三个参数`end - edata`（0x644），所以输出了1604。这个值不定主要是编译器生成的汇编指令所影响（应该）

6. Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change `cprintf` or its interface so that it would still be possible to pass it a variable number of arguments?

   ###### 题目意思是说，c语言规定参数进入stack顺序是逆序的，现在改为正序。那么，最直接的方法，就是调用`cprintf`时，逆序传入参数，这样参数在stack中的顺序就和之前一样，但很不方便。所以，我们可以更改`va_start`的实现方法，使得`ap`指向最后一个声明参数的上一个地址，而不是现在的下一个地址，而且，`va_arg`也要实现成地址--的逻辑，而不是++。

### 9.内核的栈

经过调试，内核的栈顶指针被设置在了`0xf0110000`,也就是说，栈在物理内存实际上在`0x00110000`这个地址。通过阅读`entry.S`源代码，发现内核为`stack`划分一块区域是通过声明一个`data`段的方式，通过`objdump -h obj/kern/kernel`发现 data 段的 LMA 是`0x00108000`的，所以栈的结束地址就为这个，栈大小为`0x8000`个字节（32KB）

### 10.递归函数在栈的情况

通过阅读`obj/kern/kernel.asm`,`0xf0100040`是`test_backtrace`的函数地址。每次调用`test_backtrace`就会向栈压入它的参数，而这些值分别是`5 4 3 2 1 0`

作者一步步的调试`test_backtrace`，发现它们对应的汇编指令很奇怪。首先，会有很多的`subl $0x*,$esp`，一开始我是很懵逼的，为什么要修改堆栈指针？接着调试，我发现几乎每`call Label`后，都会`addl $0x10,$esp`，也就是说又退栈 4 个双字。为什么要这样子？意义何在？是强制对齐吗？

### 11.打印栈帧信息

这个练习需要懂一些在C语言调用汇编指令的知识，详细参见 [gcc文档](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Extended-Asm) 以及 [来自codeproject网站](https://www.codeproject.com/Articles/15971/Using-Inline-Assembly-in-C-C)。我推荐后者，讲得更好。

打印 stack frame 也不是很难，只要搞清楚，进入`mon_backtrace`后，`ebp`指向的栈单元，就是上一函数调用的`ebp`值，那么上一次函数调用显然就是`test_backtrace(0)`，那么又因为每次进入`test_backtrace`，第一条指令都是`pushl %ebp`，所以，进入调用之前，参数的入栈顺序即为：`x eip ebp`，所以，`eip` = `ebp[i]` + `0x4`，其中`ebp[i]`为调用`test_backtrace(i)`的`ebp`值，`x`以此类推。 那么我们就能输出 stack frame 信息啦。详细代码可见我的仓库[monitor.c](https://github.com/astzls213/MIT6.828-2017lab/blob/master/kern/monitor.c)，有详细的注释，可以在自己写完代码再来看看。

### 12.完善栈帧信息

想做好这练习，必须了解`stab`。实际上，这个练习只需要我们补写填充行号的代码。需要看`kern/kdebug.c kern/kdebug.h inc/stab.h`的代码，所以代码量有点多（300行左右吧）。另外，这里是关于[stab_type](http://www.math.utah.edu/docs/info/stabs_12.html)的介绍。

这里简单讲一下`stab`,其实`stab`就是`symbol table`,还记得我们之前讲链接吗？这个符号表，就是将所有符号映射成虚拟地址，使得最后生成的汇编指令可以直接执行（汇编只认寄存器和内存）。`stab`的每一项除了完成这一任务，它还记录了一些其他的有用信息，比如，这个符号是一个局部变量，还是全局变量？还是一个函数名等等。

`kdebug.c`定义了两个函数：`stab_binsearch`到主要功能是：给定一个虚拟地址，和要查找的符号类型，返回一个范围？（等待填坑，作者还没完全搞懂。。）

代码实现可以见我的仓库[kdebug.c](https://github.com/astzls213/MIT6.828-2017lab/blob/master/kern/kdebug.c).

### 13.写在最后

这个实验真的让我学到了很多，感觉自己对计算机的理解有深刻了许多（从pc启动流程，到程序编译链接，再到函数递归，栈）。本文会持续更新，另外，我把自己做的实验结果放在了我的[Github仓库](https://github.com/astzls213/MIT6.828-2017lab)，里面还有一些我在学习过程查找的资料汇总，实验截图，以及本文的md文档。

