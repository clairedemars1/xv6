#include "user.h"
#include "x86.h"

typedef struct kthread_t { 
    int pid;
    void* stack;
} kthread_t;

typedef struct lock_t {
	uint flag;
} lock_t;


kthread_t thread_create( void (*start_routine) (void*), void* arg){
	kthread_t to_return;
	to_return.pid = 0;
	to_return.stack = 0;
	
	to_return.stack = malloc(2*PGSIZE);
	if (!to_return.stack){ return to_return; }
	
	to_return.stack = (char*) PGROUNDUP((uint)to_return.stack); // page align 

	(to_return.stack) += (PGSIZE); // to go to top of stack, since kernel is used to stack bottom being higher than stack top

	to_return.pid = clone(start_routine, arg, to_return.stack);
	if (to_return.pid == -1 ) {
		free(to_return.stack);
		printf(1, "\t cat thread create FAILED\n");
		exit();
	}
	return to_return; 
	
}

void thread_join(kthread_t thread){
	int ret_val;
	if ( ( ret_val = join(thread.pid) )== -1 ){
		printf(1, "\t thread_join FAILED\n");
		exit();
	}
	free(thread.stack);
}

void init_lock(lock_t* lock){
	lock->flag = 0;
}

void lock_acquire(lock_t* lock){
	while( xchg( &(lock->flag), 1) == 1)
		;
}

void lock_release(lock_t* lock){
	lock->flag = 0; // consider making assembly
}

