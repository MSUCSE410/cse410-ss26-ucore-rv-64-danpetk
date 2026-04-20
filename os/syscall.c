#include "syscall.h"
#include "console.h"
#include "const.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, va, len);
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

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	debugf("sys_read fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDIN)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
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

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
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


uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!\n");
	return fork();
}

uint64 sys_exec(uint64 va)
{
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	debugf("sys_exec %s\n", name);
	return exec(name);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}


uint64 sys_spawn(uint64 va)
{
	struct proc *p = curr_proc();
	char filename[MAX_STR_LEN];

	if (copyinstr(p->pagetable, filename, va, MAX_STR_LEN) < 0) {
		return -1;
	}

	int id = get_id_by_name(filename);
	if (id < 0) {
		return -1;
	}
	
	struct proc *np;
	if ((np = allocproc()) == 0) {
		return -1;
	}

	loader(id, np);
	np->parent = p;
	add_task(np);
	return np->pid;
}

uint64 sys_set_priority(long long prio){

	if (prio < 2) {
		return -1;
	}

	curr_proc()->prio = prio;
	return prio;
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
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
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
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_setpriority:
		ret = sys_set_priority(args[0]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
