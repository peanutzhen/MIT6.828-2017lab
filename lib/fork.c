// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern void _pgfault_upcall(void);

pte_t getPte(void *addr);
pte_t getPte(void *addr)
{
	// 没有映射就返回0即可
	// duppage只复制有映射的页
    if (!(uvpd[PDX((uintptr_t)addr)] & PTE_P))
   		return 0;
    if (!(uvpt[PGNUM((uintptr_t)addr)] & PTE_P))
		return 0;
	return uvpt[PGNUM((uintptr_t)addr)];
}

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	// LAB 4: Your code here.

	// 是写错误吗？
    if (!(err & FEC_WR))
        panic("Page fault: err = %d\nfault_va = %08x\neip = %08x\ncheck for youself!\n", err, (uintptr_t) addr, utf->utf_eip);
	
    // 现在的任务是获取一个虚拟地址
	// 这个虚拟地址是addr的pte地址

	// Get addr的pde
   	pte_t aPte = getPte(addr);

	// if addr of page not present, panic..
	if (aPte == 0)
		panic("va %08x not presented!\n", addr);

	// get perm of pte
    int perm = aPte & 0xFFF;
	
	// 如果是写错误，那这个页是写时复制页吗？
	if(!(perm & PTE_COW))
		panic("Page fault: Try to write a not COW page!\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	void *round_addr = ROUNDDOWN(addr, PGSIZE);
	//cprintf("[%08x] making new page for va %08x\n", sys_getenvid(), addr);

	// 分配一页并挂载至PFTEMP
	if ((r = sys_page_alloc(0, (void *)PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("page_alloc failed: %e\n", r);
	
	// 内容复制
	memmove((void *)PFTEMP, round_addr, PGSIZE);
	
	// 映射至新页
	r = sys_page_map(0, (void *)PFTEMP, 0, round_addr, PTE_P|PTE_U|PTE_W);
	if (r < 0)
		panic("page_map error: %e\n", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	// pn 范围检测
	if (pn >= 0xFFFFF)
		panic("pn Oversize!\n");

	// Page num to Page addr
	void *addr = (void *)(pn * PGSIZE);

	// get pte
	pte_t aPte = getPte(addr);
	
	// 获取该pte的权限位，决定下一步映射的权限
	int perm = aPte & 0xFFF;
	int ro_perm = PTE_U | PTE_P;
	int cow_perm = ro_perm | PTE_COW;

			// if 可写 或 写时复制 page
	if ((perm & PTE_W) || (perm & PTE_COW)) {
		if ((r = sys_page_map(0, addr, envid, addr, cow_perm)) < 0)
			panic("map failed with %d\n", r);
		// 修改回父进程的权限，使其没有PTE_W，但必须有PTE_COW
		if ((r = sys_page_map(0, addr, 0, addr, cow_perm)) < 0)
			panic("remap failed with %d\n", r);
	}
	else {	// 只读页面
		if ((r = sys_page_map(0, addr, envid, addr, ro_perm)) < 0)
			panic("map failed with %d\n", r);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	// 安装 pgfault
	set_pgfault_handler(pgfault);
	
	// 无性繁殖一波
	envid_t child = sys_exofork();
	
	// 设置分支
	if (child < 0)			// 繁殖失败
		panic("fork failed! error code = %d\n", child);
	else if(child == 0) {	// 子进程的出厂设置
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	else {					// 父进程的产后护理
		int rtv;			// 用于返回错误码

		// 使子进程映射自己的页 below UTOP
		// 但是UXSTCK是每个子进程独自拥有一张新的，所以不必映射他。
		uint8_t *addr = (uint8_t*) 0;
		uint8_t *end = (uint8_t *)UXSTACKTOP - PGSIZE;
		pte_t tmp;

		for (; addr < end; addr += PGSIZE)
		{
			
			tmp = getPte(addr);
			if (tmp == 0)	// 若父进程没有映射(PTE_P == 0)，则跳过
				continue;
			else			// 否则duppage
				duppage(child, PGNUM(addr));
		}
		// 为子进程设置异常栈和upcall地址
		rtv = sys_page_alloc(child, (void *) UXSTACKTOP - PGSIZE, PTE_P|PTE_U|PTE_W);
		if (rtv < 0)
			panic("child exception stack alloc failed!\n");
		sys_env_set_pgfault_upcall(child, _pgfault_upcall);

		// 子进程现在可以开始运行
		rtv = sys_env_set_status(child, ENV_RUNNABLE);
		if (rtv < 0)
			panic("set status failed!\n");
		return child;
	}
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
