

typedef struct kthread_t { 
    int pid;
} kthread_t;

//~ lock_aquire
//~ lock_release
//~ init_lock(&lock)?

//~ void func ( void (*f)(int) );
//~ This states that the parameter f will be a pointer to a function which has a void return type and which takes a single int parameter.

kthread_t thread_create( void (*start_routine) (void*), *void arg){
	
}

thread_join(kthread_t thread){
	
}

lock_init(lock_t* lock){
	
}

lock_aquire(lock_t* lock){
	
}

lock_release(lock_t* lock){
	
}
