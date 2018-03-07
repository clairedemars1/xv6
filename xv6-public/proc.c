#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "procinfo.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC]; // actually procs plural
  struct spinlock all_heaps_lock;
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&ptable.all_heaps_lock, "lock all heaps for sake of threads");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
	// I checked it's called for each proc, even forked ones
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  
  // initialize process's record of shared_pages
	int i;
	for(i=0; i<NSH; i++){
		p-> shared_pages[i].virtual_addr = 0;
	}

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE; // go to the bottom of the stack

  // Leave room for trap frame.
  sp -= sizeof *p->tf; // move down, past a trapframe sized chunk
  p->tf = (struct trapframe*)sp; // tell the new proc that it's trapframe is in the chunk just passed

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret; // put the function trapret's memory address at the next spot in the stack

  sp -= sizeof *p->context; // ie *(p->context);  // move down, past a chunk the size of a context struct
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context); // zero out the context
  p->context->eip = (uint)forkret; // set the context's instruction pointer to the forkret 
  
  
  return p;
  
  // summary, we set up directions to 2 functions
  // trapret (in the stack pointer of the trapframe), presumably for the user thread
  // forkret (in the instruction pointer of the context), presumably for the kernal thread
}


//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

#define should_use_global_lock 0 // on Tues this caused panic, now it's fine
#define should_specific 1
// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	uint sz;
	struct proc *curproc = myproc();

	#if should_use_global_lock
	acquire(&ptable.all_heaps_lock); 
	#endif
	#if should_specific
	acquire(curproc->heap_lock_pointer);
	#endif
	//~ int i; // trying to get kernel locks failing test case
	//~ for(i=0; i<2000; i++);
	sz = curproc->sz;
	if(n > 0){
		if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
			#if should_use_global_lock
			release(&ptable.all_heaps_lock);
			#endif
			#if should_specific
			release(curproc->heap_lock_pointer);
			#endif
			return -1;
		}
	} else if(n < 0){
		if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0){
			#if should_use_global_lock
			release(&ptable.all_heaps_lock);
			#endif	
			#if should_specific
			release(curproc->heap_lock_pointer);
			#endif
			
		    return -1;
		}
	}
	
	curproc->sz = sz;
	#if should_use_global_lock
	release(&ptable.all_heaps_lock);
	#endif
	#if should_specific
		release(curproc->heap_lock_pointer);
	#endif
	switchuvm(curproc);
	
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  np->is_thread = 0; //NEW
  initlock(&np->heap_lock, "heap lock"); //NEW
  np->heap_lock_pointer = &np->heap_lock; // NEW
  
  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz, np)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
	// exit is not called at the death of every process 
	// (see init.c where we learn that most processes are ended by a wait call)
  
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  if ( !(curproc->is_thread) ){ //NEW
	  for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd]){
		  fileclose(curproc->ofile[fd]);
		  curproc->ofile[fd] = 0;
		}
	  }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid; // havekids means has process kids, it doesn't acknowledge thread kids
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children. Process repeats until have found one (intersperced with sleep)
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc || p->is_thread)
        continue; 
      havekids = 1;
      //~ cprintf("\tfound a kid to clean up with pid: %d\n", curproc->pid);
      if(p->state == ZOMBIE ){ // so a bad zombie is one with a dead parent
        // Found one.
        pid = p->pid;
        kfree(p->kstack); // freeing memory
        p->kstack = 0;
        freevm(p->pgdir, p); // freeing memory
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{	
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void test(void){
	cprintf("test called");
}

int 
getprocsinfo(struct procinfo* info){
	// assumes info is already allocated with 64 slots
	
	int count = 0;
	int max_num_proc = NPROC;
	struct proc* p;
	
    acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[max_num_proc]; p++){
		enum procstate state = p->state;
		if (state == EMBRYO 
			|| state == SLEEPING
			|| state == RUNNABLE
			|| state == RUNNING
			|| state == ZOMBIE
		){ // ignoring processes in state UNUSED
			info[count].pid = p->pid;
			safestrcpy(info[count].name, p->name, 16); // can't point to same address since memory is kernal-access only
			count++;
		}
	}	
	release(&ptable.lock);
		
	// note to self: try to use kalloc, to allocate to the kernal
		// to verify that  it fails (since the usertests are not run in priviledged mode, they can't get to that memory)
		//~ info = (struct procinfo*) malloc(count* sizeof(struct procinfo) ); 
	return count;
}

void call_kernal_version(void){
	cprintf("call_kernal_version was called\n");
}


int clone(void (*fcn) (void*), void *arg, void*stack){
	// make a thread
	
	// based on fork (DIFF shows where they differ)
	// basically, make a new process, but sharing the pgdir of the calling process
	// and using the passed user stack
	// with execution as if it had just called the function fcn with the arg arg
	
	int i, pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if((np = allocproc()) == 0){
		cprintf("could not allocate a thread\n");
		return -1;
	}
	np->is_thread = 1; //DIFF
    np->heap_lock_pointer = curproc->heap_lock_pointer; //DIFF

	np->pgdir = curproc->pgdir; // DIFF: use the same pgdir, don't make a copy of it
	np->sz = curproc->sz; // ok b/c sz points to top of heap, and we're not messing with the heap, just the stack 
	np->parent = curproc;
	
	*np->tf = *curproc->tf; // same as *(np->tf) // note: struct trapframe *tf;  
	
	np->tf->eip = (uint) fcn;  // DIFF
	//~ np->tf->ebp = (uint) stack;  // DIFF
	
	// put arg and return address into the stack (based on exec)
		// stack points to the bottom of the stack (high memory end)
	stack -= sizeof(uint);
	*( (uint*) stack) = (uint) arg;

	stack -= sizeof(uint);
	*( (uint*) stack) = 0xffffffff; 

	np->tf->esp = (uint) stack;  // DIFF

	//shared memory info
	for (i=0; i< NSH; i++){
		np->shared_pages[i] = curproc->shared_pages[i];
	}

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0; //?
	
	for(i = 0; i < NOFILE; i++)
	if(curproc->ofile[i])
	  np->ofile[i] = filedup(curproc->ofile[i]);
	  //~ np->ofile[i] = curproc->ofile[i]; // I wanted to change this, but it broke stuff if I ran the same test multiple times
	np->cwd = idup(curproc->cwd);
	//~ np->cwd = curproc->cwd; //

	safestrcpy(np->name, "thread", sizeof("thread")); // DIFF

	pid = np->pid;

	acquire(&ptable.lock);

	np->state = RUNNABLE;

	release(&ptable.lock);

	return pid;
}

int join(int pid){
	// return 0 for success, -1 for failure
	// based on wait 
	
	struct proc *p;
	int thread_child_with_given_pid_exists;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for(;;){ // scan through the table a bunch of times
		// Scan through table looking for exited children. Process repeats until have found one (intersperced with sleep)
		thread_child_with_given_pid_exists = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		  
			if(p->parent != curproc || !p->is_thread || (p->pid != pid) )
				continue; // to the next process in the table
			thread_child_with_given_pid_exists = 1;
			if(p->state == ZOMBIE ){ 
				// Found one.
				kfree(p->kstack);
				p->kstack = 0;
				//~ freevm(p->pgdir, p); // DIFFERENT: don't free user memory, cuz this is a thread
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				p->is_thread = 0;
				release(&ptable.lock);
				return 0; // only way to succeed
			}
		}
	
		// at this point, we've looked at all process, if the one we're looking for isn't there, then fail 
		if(!thread_child_with_given_pid_exists || curproc->killed){
		  release(&ptable.lock);
		  return -1; // only way to fail
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock);  //DOC: wait-sleep
	}
	cprintf("done with clone\n");
}
