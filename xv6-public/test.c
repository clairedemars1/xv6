#include "types.h"
#include "user.h"

int stdout = 1;

void deref_null(){
	char* ptr = 0;
	printf(stdout, "val at ptr: %d\n", *ptr);
}

void share_memory_basic(){
	char* test_str = "I am Bob.\n";
	int pid = fork();
	if(pid < 0){
		printf(stdout, "Fork failed\n");
		exit();
	}
	else if (pid == 0){ // child
		// runs before parent, b/c parent is waiting (or does parent need to tell child to sleep?)
			// does it matter that what the child does is an io action? Can that action last after the child is dead?
		char* shared_page = (char*) shmem_access(2);
		// //~ shared_page = "i ii iiiii"; // todo
		
		strcpy(shared_page, test_str);
		//~ printf(stdout, "did it work?:%s\n", shared_page); // it worked
		
		printf(stdout, "child finished\n");
		exit();
	} else { // parent
		wait(); // until child has written 
		
		char *shared_page = (char *) shmem_access(2);
		// //~ shared_page = test_str;
		//~ printf(stdout, "%s", shared_page);
		if (strcmp(shared_page, test_str) == 0){ 
			printf(stdout, "PASSED share memory basic test\n");
		} else {
			printf(stdout, "\tFAILED share memory basic test\n");
		}
		printf(stdout, "parent finished\n");
	}
}

void share_memory_fork_after_get_page_access(){
	
}

// make sure all children exit
int
main(int argc, char *argv[])
{
  printf(stdout, "Starting proj 2 tests\n");
  //~ deref_null();
  share_memory_basic();
  exit();
}
