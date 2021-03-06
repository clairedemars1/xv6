/*
 * Here is a mock producer/consumer problem. It should easily expose most basic lock or thread issues.
 * Flip LOCKS_ON to 0 to disable locks. When on, the locks can help you debug locking problems.
 * When off, it can help you debug multi-threading problems.
 *
 * QEMU by default will only use 1 host thread for n virtual CPUs. To change this, you need to add 
 * --enable-kvm to the qemu conf line in the xv6 Makefile. This requires the kvm kernel module that
 *  you may need to set up if you are using your own host machine. I also recommend changing the
 *  default Makefile setting of 2 CPUs to at least 4, but you really only need more than 1. On
 *  a real machine, just 1 CPU should be more than enough to expose lock issues but xv6 + qemu
 *  will not reveal locking issues on a single thread. IF YOU DON'T ENABLE KVM AND 2+ VCPUS THEN YOUR 
 *  PROGRAM MAY NOT ACTUALLY BE WORKING.
 *
 * Please match your API to fit this file, not the other way around...
 *
 * DISCLAIMER: This tester may not identify all issues. Please be sure to think about any other cases
 * that might be possible. Hint: There are some things that can be tested by making very small additions
 * to this file. What are they?
 *
 *
 * Tyler Allen 2018-02-14
 */


#include "types.h"
#include "stat.h"
#include "user.h"
#include "kthreads.h"
#include "thread_lib.h"
#include "procinfo.h"

#define LOCKS_ON 1
#define NULL 0



void print_procs(){
	struct procinfo procs[64];
	int proc_count = getprocsinfo(procs);
	int i;
	for (i=0; i<proc_count; i++){
		printf(1, "\tpid: %d, name: %s\n", procs[i].pid, procs[i].name);
	}
}


void slow(void* arg){
	printf(1, "\t slow ran with arg %d\n", *(int*)arg); 
	sleep(300);
	exit();
}

void when_main_process_calls_join_it_actually_waits(){
    int i = 3;
    kthread_t thread = thread_create(slow, &i);
    // sleep(300); // necessary before join worked, to prevent lllll 
    thread_join(thread);
    printf(1, "\tshould not print until bar is done\n");
}

void fast(void* arg){
	printf(1, "\tfast ran with arg %d\n", *(int*)arg); 
	exit();
}

void join_cleans_up_procs(){
	int i = 3;
    kthread_t thread = thread_create(fast, &i);
    print_procs();
    thread_join(thread);
    print_procs();
}

// global lock for product
lock_t lock;
// our product
int things = 0;
int things_made = 0;


#define NUM_PROD 3
#define NUM_CONS 2
#define TOTAL_PRODUCTS 10000000 // 10,000,000   //  NUM_CONS*MAXCONSUMED
//~ #define TOTAL_PRODUCTS 10
void producer(void* arg)
{
    int cont = 1;
    while (cont)
    {
        #if LOCKS_ON
        lock_acquire(&lock);
        #endif
        // magical producing algorithm
        if (things_made < TOTAL_PRODUCTS)
        {
            ++things;
            ++things_made;
        }
        else
        {
            cont = 0;
        }
        #if LOCKS_ON
        lock_release(&lock);
        #endif
    }
    //~ printf(1, "done with producer\n");
    exit();
}

#define MAX_CONSUME 3000000 // 3,000,000
//~ #define MAX_CONSUME 3
void consumer(void* arg)
{
	//~ printf(1, "consumer got called\n"); // changes execution
    int i;
    int consumed = 0;
    // dumb little busy sleep
    for (i = 0; i < 200; i++);
    
    while (consumed < MAX_CONSUME)
    {
        // not thread safe but give producers time
        while(things <= 0);
        
		#if LOCKS_ON
		lock_acquire(&lock);
		#endif
		//~ printf(1, "consumerm#: %d, things: %d, consumed: %d\n", * (int*) arg, things, consumed);
		//~ printf(1, "FISH\n");

		if (things > 0)
		{
			// useful consumption of resources algorithm
			// guaranteed optimal usage
			--things;
			++consumed;
			//~ printf(1, "pidgeon\n");
        }
        
        //~ ++consumed; //added
        #if LOCKS_ON
        lock_release(&lock);
        #endif
    }
    //~ printf(1, "DONE\n");
    //~ printf(1, "consumer %d consumed: %d\n", *(int*)arg, consumed);
    exit();
}

void make_two_threads(){
	int i = 3;
	kthread_t t1 = thread_create(fast, &i);
	kthread_t t2 = thread_create(fast, &i);
	thread_join(t1);
	thread_join(t2);
}

void orig_test(){
	int i;
    init_lock(&lock);
    int indices[NUM_CONS];
    kthread_t producers[NUM_PROD];
    kthread_t consumers[NUM_CONS];
    for (i = 0; i < NUM_CONS; i++)
    {	
		// printf(1, "making consumer #%d\n", i);
        indices[i] = i;
        consumers[i] = thread_create(consumer, &indices[i]);
    }
    
    for (i = 0; i < NUM_PROD; i++)
    {
		// printf(1, "making producer#%d\n", i);
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
    printf(1, "Remaining products: %d\n", things); 
    printf(1, "Things made: %d\n", things_made); 
    #define REMAINING (int)(TOTAL_PRODUCTS - NUM_CONS * (double)MAX_CONSUME)
    if (things != (REMAINING))
    {
        printf(1, "Lock/thread issue detected, should be %d things left\n", REMAINING);
    }
    else
    {
        printf(1, "Test passed!\n");
    }
}


int main(void)
{
	//~ printf(1, "starting test\n");
	join_cleans_up_procs();
	//~ when_main_process_calls_join_it_actually_waits();
	//~ make_two_threads(); // works fine
	//~ orig_test();
	//~ printf(1, "about to exit test process\n");
    exit();
}
