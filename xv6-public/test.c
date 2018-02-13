#include "types.h"
#include "user.h"
#include <stdint.h>

int stdout = 1;

void print_test_result(int passed, char* name){
	if (passed){
		printf(stdout, "Passed %s\n", name);
	} else {
		printf(stdout, "\tFAILED %s\n", name);
	}
}

void deref_null(){
	
	char* ptr = 0;
	
	printf(stdout, "now");
	*ptr = 'a'; // trap
	//~ printf(stdout, ptr); // trap
	//~ char t = *ptr; // linux trap, no trap in my code
	//~ t++;
}


void share_memory_basic(){ // fork before get page access
	char* test_str = "Ann\n";
	int pid = fork();
	if(pid < 0){
		printf(stdout, "Fork failed\n");
		exit();
	}
	else if (pid == 0){ // child goes first: writes
		char* shared_page = (char*) shmem_access(2);
		// //~ shared_page = "i ii iiiii"; // todo
		
		strcpy(shared_page, test_str);
		//~ printf(stdout, "did it work?:%s\n", shared_page); // it worked
		
		printf(stdout, "child finished\n");
		exit();
	} else { // parent goes second: reads
		wait();
		
		char *shared_page = (char *) shmem_access(2);
		// //~ shared_page = test_str;
		//~ printf(stdout, "%s", shared_page);
		if (strcmp(shared_page, test_str) != 0){ 
			printf(stdout, "PASSED share memory basic test\n");
		} else {
			printf(stdout, "\tFAILED share memory basic test\n");
		}
	}
}

void ref_counts_after_process_exits(){
	char* name = "ref_counts_after_process_exits";
	int pid = fork();
	if (pid == 0){ // child
		shmem_access(1);
		exit();
	} else {
		wait();
		if( shmem_count(1) != 0){
			print_test_result(0, name);
			return; 
		}
		print_test_result(1, name); 
	}
}

void basic_ref_counts(){
	char* name_of_test = "basic_ref_counts";
	char* shared_page_2 = (char*) shmem_access(2);
	shared_page_2 = (char*) shmem_access(2);
	if( !shared_page_2 ){ 
		printf(stdout, "FAILED b/c allocation failed\n");
		return;
	}
	
	if( shmem_count(2) != 1 || shmem_count(0) != 0){ 
		print_test_result(0, name_of_test); 
		return;
	}
	
	print_test_result(1, name_of_test);
}

void share_memory_fork_after_get_page_access(){
	
}

void use_memory_not_in_page_table(){
	int* ptr = UINTPTR_MAX;
	(*ptr)++; // throws trap 14 :)
}

// Note: My code zeros-out a page once it's reference count is zero
// for example, assuming no other processes,
// if a program forks, then the child gets access then write and exits
// and the parent waits until the child has exited then get access and read
// then the parent should find nothing. Because when the child died, 
// the shared page would have a reference count of zero 
// (assuming no other processs are sharing it) and so it should be gotten rid of


// make sure all children exit
int
main(int argc, char *argv[])
{
  //~ printf(stdout, "Starting proj 2 tests\n");
  //~ deref_null();
  //~ share_memory_basic();
  basic_ref_counts();
  ref_counts_after_process_exits();
  
  //~ use_memory_not_in_page_table(); // for understanding
  exit();
}
