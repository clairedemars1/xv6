#include "user.h"

typedef struct kthread_t { 
    int pid;
    void* stack;
} kthread_t;

typedef struct lock_t {
	// can just be a mutex
} lock_t;


kthread_t thread_create( void (*start_routine) (void*), void* arg){
	kthread_t to_return;
	to_return.pid = 0;
	to_return.stack = 0;
	
	char* stack = malloc(2*PGSIZE);
	if (!stack){ return to_return; }
	printf(1, "stack: %d \n", stack);
	
	stack = (char*) PGROUNDUP((uint)stack); // page align 
	printf(1, "stack: %d \n", stack);

	stack += (PGSIZE - 1); // to go to top of stack, since kernel is used to stack bottom being higher than stack top
	printf(1, "stack: %d \n", stack);

	to_return.pid = clone(start_routine, arg, stack);
	if (to_return.pid == -1 ) {
		free(stack);
		printf(1, "\t thread create FAILED\n");
		exit();
	}
	return to_return; 
	
}

void thread_join(kthread_t thread){
	if ( !join(thread.pid) ){
		printf(1, "\t thread_join FAILED\n");
		exit();
	}
}

void init_lock(lock_t* lock){
	
}

void lock_acquire(lock_t* lock){
	
}

void lock_release(lock_t* lock){
	
}
