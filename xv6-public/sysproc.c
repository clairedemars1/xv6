#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "procinfo.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


int sys_getprocsinfo(void){

	char* info;
	if ( argptr(0, &info, sizeof(struct procinfo) * NPROC) < 0 ){
		return -1;
	}
	cprintf("info is %d\n", info);
	return getprocsinfo( (struct procinfo*) info);
	return 1;
}

void sys_call_kernal_version(void){
	
	call_kernal_version();
}

int sys_clone(void){
	void (*fcn)(void*); 
	void *arg, *stack;
	if ( argptr(0, (char**) &fcn, sizeof(fcn) ) < 0 ){ //?
		return -1;
	}
	if ( argptr(1, (char**) &arg, sizeof(arg) ) < 0 ){ //?
		return -1;
	}
	if ( argptr(2, (char**) &stack, PGSIZE) < 0 ){
		return -1;
	}
	return clone(fcn, arg, stack);
}

int sys_join(void){
	int pid;
	if ( argint(0, &pid) < 0) return 0;
	return join(pid);
}
