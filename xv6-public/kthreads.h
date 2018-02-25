

typedef struct kthread_t { 
    
} kthread_t;

thread_create(void* fcn_name, kthread_t* 
lock_aquire
lock_release
//~ init_lock(&lock)?

void func ( void (*f)(int) );
This states that the parameter f will be a pointer to a function which has a void return type and which takes a single int parameter.


int i;
    init_lock(&lock);
    int indices[NUM_CONS];
    kthread_t producers[NUM_PROD];
    kthread_t consumers[NUM_CONS];
    for (i = 0; i < NUM_CONS; i++)
    {
        indices[i] = i;
        consumers[i] = thread_create(consumer, &indices[i]);
    }
    for (i = 0; i < NUM_PROD; i++)
    {
        producers[i] = thread_create(producer, NULL);
    }
    for (i = 0; i < NUM_PROD; i++)
    {
        thread_join(producers[i]);
    }
    for (i = 0; i < NUM_CONS; i++)
    {
        thread_join(consumers[i]);
    }
