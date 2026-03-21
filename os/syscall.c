#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	TimeVal pval;
	uint64 cycle = get_cycle();
	pval.sec = cycle / CPU_FREQ;
	pval.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	return copyout(curr_proc()->pagetable, (uint64)val, (char*)&pval, sizeof(pval));
}

int sys_task_info(TaskInfo* ti) {
	struct proc *target_proc = curr_proc();
	if (target_proc->start_time < 0) {
		return -1;
	}

	uint64 time_ms = get_msec();
	printf("time_ms: %d, tp start time: %d", time_ms, target_proc->start_time);
	int time_elapsed_ms = time_ms - target_proc->start_time;
	if (time_elapsed_ms < 0) {
		return -1;
	}

	TaskInfo pti;
	pti.status = Running;
	pti.time = time_elapsed_ms;
	memmove(pti.syscall_times, target_proc->syscall_times, sizeof(pti.syscall_times));
	
	return copyout(curr_proc()->pagetable, (uint64)ti, (char*)&pti, sizeof(pti));
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)

int sys_mmap(void* start, unsigned long long len, int port, int _flag, int _fd) {
	// see if port valid
	if (((port & ~0x7) != 0) || ((port & 0x7) == 0)) {
		return -1;
	}
	
	// make sure start is aligned
	if ((((uint64)start) % PAGE_SIZE) != 0) {
		return -1;
	}

	uint64 start_va = (uint64)start;
	uint64 end_va = PGROUNDUP(start_va + len);
	uint64 cva;
	pagetable_t pagetable = curr_proc()->pagetable;
	
	int perm = PTE_U;
	if (port & 1) { perm |= PTE_R; }
	if (port & 2) { perm |= PTE_W; }
	if (port & 4) { perm |= PTE_X; }

	for(cva = start_va; cva < end_va; cva += PAGE_SIZE) {
		void* page = kalloc();
		if (page == 0) {
			return -1;
		}
		memset(page, 0, PGSIZE);

		if (mappages(pagetable, cva, PAGE_SIZE, (uint64)page, perm) != 0) {
			return -1;
		}
	}

	return 0;	
}

int sys_munmap(void* start, unsigned long long len) {

	// make sure start is aligned
	if ((((uint64)start) % PAGE_SIZE) != 0) {
		return -1;
	}

	uint64 start_va = (uint64)start;
	uint64 end_va = PGROUNDUP(start_va + len);
	uint64 cva;
	pagetable_t pagetable = curr_proc()->pagetable;

	for(cva = start_va; cva < end_va; cva += PAGE_SIZE) {
		if (useraddr(pagetable, cva) == 0) {
			return -1;
		}

		uvmunmap(pagetable, cva, 1, 1); 
	}

	return 0;	
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);

	curr_proc()->syscall_times[id]++; // update syscall counter

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap((void*) args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void*) args[0], args[1]);
		break;
	default:	
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
