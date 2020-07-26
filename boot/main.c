#include <inc/x86.h>
#include <inc/elf.h>

/**********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(boot.S and main.c) is the bootloader.  It should
 *    be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in boot.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 **********************************************************************/

#define SECTSIZE	512
#define ELFHDR		((struct Elf *) 0x10000) // scratch space

void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void
bootmain(void)
{
	// 两个指向程序头的指针，一个指向首个proghdr，一个指向末尾的
	struct Proghdr *ph, *eph;

	// read 1st page off disk
	// 一开始我问网上，为什么这里要读4096字节呢？很多人回答模糊不清。
	// 其实，这里只要你读超过150B就不会有问题（理论上）,而readseg至少
	// 读一个扇区到内存，所以这里调成不为0的值就行。
	// 而读4096B呢，我后来看了下第一个程序段在磁盘的位置，发现他是在距离
	// 第二个扇区的0x1000个字节偏移量（即4096B），即第10个扇区sector 9
	// 那么，这里读4096B，就是讲磁头停留在第一个将要被read的程序段，减少
	// 寻道时间。当然，这只是我的猜想。
	readseg((uint32_t) ELFHDR, 96, 0);

	// is this a valid ELF?
	if (ELFHDR->e_magic != ELF_MAGIC)
		goto bad;

	// load each program segment (ignores ph flags)
	// 现将ELFHDR指针告诉编译器当做byte指针看待，那么+1时就是对地址加1B
	// e_phoff为第一个程序头距离ELF的字节偏移量
	// 再告诉编译器看作指向程序头的指针，这样就可以用->访问它的成员
	ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	eph = ph + ELFHDR->e_phnum;		// 指向最后一个proghdr的末尾
	for (; ph < eph; ph++)
		// p_pa is the load address of this segment (as well
		// as the physical address)
		// 这里的offset，是相对于elfhdr（位于sector 1）来说的（字节单位）
		// readseg的任务，就是从sector 1(0 indexd)偏移offset个字节开始
		// 读取memsz个字节，到p_pa去
		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

	// call the entry point from the ELF header
	// note: does not return!
	// 将e_entry转化为函数指针并执行
	((void (*)(void)) (ELFHDR->e_entry))();

bad:
	outw(0x8A00, 0x8A00);
	outw(0x8A00, 0x8E00);
	while (1)
		/* do nothing */;
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked
void
readseg(uint32_t pa, uint32_t count, uint32_t offset)
{
	uint32_t end_pa;		// 声明结束的物理地址

	end_pa = pa + count;	// 定义结束物理地址=起始地址+待读取字节数

	// round down to sector boundary
	// 这里有必要解释一下，简单来说，就是将起始地址给对齐一个扇区大小
	// SECTSIZE - 1 = 511 = 1 1111 1111，再去反，变成0xfffffe00
	// 这样，pa的高23bit不变，低9bit全为0，这样就实现了对齐
	pa &= ~(SECTSIZE - 1);

	// translate from bytes to sectors, and kernel starts at sector 1
	// 这里的offset是程序段距离ELF头的字节偏移量
	// 而ELF头在第二个扇区，即sector 1(0开始计数）
	offset = (offset / SECTSIZE) + 1;

	// If this is too slow, we could read lots of sectors at a time.
	// We'd write more to memory than asked, but it doesn't matter --
	// we load in increasing order.
	// 他这里这样实现，会读取多余实际请求的字节数,而且如果pa对齐后
	// 若offset和pa对齐的偏移量不等，就会错位了，也就是说，pa指向的地方
	// 并不是所期望的代码数据。只是个人见解，可能有误。
	while (pa < end_pa) {
		// Since we haven't enabled paging yet and we're using
		// an identity segment mapping (see boot.S), we can
		// use physical addresses directly.  This won't be the
		// case once JOS enables the MMU.
		// 将第offset个扇区写到对齐了的pa
		readsect((uint8_t*) pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void
waitdisk(void)
{
	// wait for disk ready
	// Addr 0x1F7 is Status register.
	//  7	  6 	5	 4	  3	  2  	1 	  0
	// Busy Ready Fault Seek DRQ CORR IDDEX Error
	while ((inb(0x1F7) & 0xC0) != 0x40)
		/* do nothing */;
}

void
readsect(void *dst, uint32_t offset)
{
	// wait for disk to be ready
	waitdisk();

	outb(0x1F2, 1);				// count = 1,代表只读一个扇区
	outb(0x1F3, offset);		// 0x1F2~0x1F6 是LBA寻址模式
	outb(0x1F4, offset >> 8);	// 更多细节可以看我的github仓库，有ATA's
	outb(0x1F5, offset >> 16);	// API介绍,at astzls213/MIT6.828-2017lab
	outb(0x1F6, (offset >> 24) | 0xE0);
	outb(0x1F7, 0x20);	// cmd 0x20 - read sectors

	// wait for disk to be ready
	waitdisk();

	// read a sector
	insl(0x1F0, dst, SECTSIZE/4);
}

