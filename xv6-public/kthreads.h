#include "user.h"
#include "x86.h"

typedef struct kthread_t { 
    int pid;
    //~ void* stack;
    void* to_free; // the memory surrounding the stack
} kthread_t;

typedef struct lock_t {
	uint flag;
} lock_t;


kthread_t thread_create( void (*start_routine) (void*), void* arg){
	printf(1, "inside thread_create\n");
	kthread_t to_return;
	void* stack = 0;
	to_return.pid = 0;
	//~ to_return.stack = 0;
	printf(1, "before malloc\n");
	stack = malloc(2*PGSIZE);
	
	printf(1, "after malloc\n");
	if (!stack){ 
		printf(1, "thread create returning early\n");
		return to_return;
	}
	to_return.to_free = stack;
	
	stack = (char*) PGROUNDUP((uint)stack); // page align 

	(stack) += (PGSIZE); // to go to top of stack, since kernel is used to stack bottom being higher than stack top
	printf(1, "before clone call\n");
	to_return.pid = clone(start_routine, arg, stack);
	printf(1, "after clone call\n");
	if (to_return.pid == -1 ) {
		free(stack);
		printf(1, "\t cat thread create FAILED\n");
		exit();
	}
	printf(1, "thread created succeeded\n");
	return to_return; 
	
}

void thread_join(kthread_t thread){
	int ret_val;
	if ( ( ret_val = join(thread.pid) )== -1 ){
		printf(1, "\t thread_join FAILED\n");
		exit();
	}
	free(thread.to_free);
}

void init_lock(lock_t* lock){
	lock->flag = 0;
}

void lock_acquire(lock_t* lock){
	while( xchg( &(lock->flag), 1) == 1)
		;
}

void lock_release(lock_t* lock){
	//~ lock->flag = 0; // consider making assembly
	asm volatile("movl $0, %0" : "+m" (lock->flag) : );
}

