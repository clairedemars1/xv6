#include "types.h"
#include "user.h"
#include <stdint.h>

int stdout = 1;

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
		if (strcmp(shared_page, test_str) == 0){ 
			printf(stdout, "PASSED share memory basic test\n");
		} else {
			printf(stdout, "\tFAILED share memory basic test\n");
		}
	}
}

void share_memory_test1_write_and_read_from_same_process(){
	char* test_str = "Ann\n";
	char* shared_page = (char*) shmem_access(0);
	if( !shared_page ){ 
		printf(stdout, "FAILED b/c allocation failed\n");
		return;
	}
	strcpy(shared_page, test_str);
}

void share_memory_fork_after_get_page_access(){
	
}

void use_memory_not_in_page_table(){
	int* ptr = UINTPTR_MAX;
	(*ptr)++; // throws trap 14 :)
}

// make sure all children exit
int
main(int argc, char *argv[])
{
  //~ printf(stdout, "Starting proj 2 tests\n");
  //~ deref_null();
  share_memory_test1_write_and_read_from_same_process();
  //~ share_memory_basic();
  
  //~ use_memory_not_in_page_table(); // for understanding
  exit();
}
