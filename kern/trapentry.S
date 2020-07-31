/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
/* 我们的中断处理程序就被定义在这里
 * 可以看到，中断事件的处理流程是：
 * 获得中断向量码 --> 获取 interrupt/trap gate --> 用sel在GDT获取
 * 中断处理程序所在段基址 --> 与off相加得到线性地址 --> 切换栈（
 * 如果需要） --> 保存信息 --> 到达这里定义的标号 --> ...
 * 
 * 请在spring_knowledges文件夹获取更详细的调用流程。
 */
/* processor defined */
TRAPHANDLER_NOEC(DIV, T_DIVIDE)
TRAPHANDLER_NOEC(DEBUG, T_DEBUG)
TRAPHANDLER_NOEC(NMI, T_NMI)
TRAPHANDLER_NOEC(BREAKPOINT, T_BRKPT)
TRAPHANDLER_NOEC(OVERFLOW, T_OFLOW)
TRAPHANDLER_NOEC(OUT_BOUND, T_BOUND)
TRAPHANDLER_NOEC(INVALID_OP, T_ILLOP)
TRAPHANDLER_NOEC(UNAVA_DEVICE, T_DEVICE)
TRAPHANDLER(DOUBLE_FAULT, T_DBLFLT)
TRAPHANDLER(INVALID_TSS, T_TSS)
TRAPHANDLER(SEG_MISS, T_SEGNP)
TRAPHANDLER(STACKSEG_FAULT, T_STACK)
TRAPHANDLER(NO_PERMISSION, T_GPFLT)
TRAPHANDLER(PAGE_FAULT, T_PGFLT)
TRAPHANDLER_NOEC(MATH_FAULT, T_FPERR)
TRAPHANDLER(ALIGN_CHECK, T_ALIGN)
TRAPHANDLER_NOEC(MACHINE_CHECK, T_MCHK)
TRAPHANDLER_NOEC(SIMD_EXC, T_SIMDERR)
/* user defined */
TRAPHANDLER_NOEC(SYSCALL, T_SYSCALL)
TRAPHANDLER_NOEC(DEFAULT, T_DEFAULT)


/*
 * Lab 3: Your code here for _alltraps
 */
/* 我们秩序向 stack 压如 ds 和 es。
 * 因为 在调用_alltraps前， int指令和对应的中断向量入口程序
 * 已经帮助我们压入所需信息，比如int：亚入 ss, esp, eflags, cs, eip,
 * error_code(if so)
 */
_alltraps:
	pushl %ds
	pushl %es

	pushal
	
	movw $GD_KD,%ax
	movw %ax,%ds
	movw %ax,%es
	
	pushl %esp
	call trap
	/* trap never return, should use jmp instead of call */
