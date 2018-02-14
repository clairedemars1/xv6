#include "types.h"
#include "user.h"
#include <stdint.h>

int stdout = 1;
int running_ref_counts[4];

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

/*
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

*/

void ref_counts_after_process_exits(){
	char* name = "ref_counts_after_process_exits";
	int pg_num = 1; // when both this and later test used 1, later test failed
	int pid = fork();
	if (pid == 0){ // child
		shmem_access(pg_num);
		running_ref_counts[pg_num]++;
		if (shmem_count(pg_num) != running_ref_counts[pg_num] ){
			print_test_result(0, name);
		}
		running_ref_counts[pg_num]--; // b/c we're exiting
		/*int pid2 = fork();
		if (pid2 == 0){ // child
			
			printf(stdout, "exit grandchild\n");
			exit();
		} else {
			wait();
		}*/
		//~ printf(stdout, "exit child\n");
		exit();
	} else {
		wait();
		
		if( shmem_count(pg_num) != running_ref_counts[pg_num]){ 
			print_test_result(0, name);
			return; 
		}
		print_test_result(1, name); 
	}
}

void two_processes_simultaneously__fork_is_irrelevant(){
	char* name = "two_processes_simultaneously__fork_is_irrelevant";
	char* test_str = "Ann\n";
	int pg_num = 3;
	int pid = fork();
	if (pid == 0){ // child
		sleep(200); // let parent go first
		char* childs_shared_page = shmem_access(pg_num);
		running_ref_counts[pg_num]++;
		if( shmem_count(pg_num) != running_ref_counts[pg_num]){
			print_test_result(0, name);
			printf(stdout, "case 1\n");
			return; 
		}
		if (strcmp(childs_shared_page, test_str) != 0){ 
			print_test_result(0, name);
			printf(stdout, "case 2\n");
			return; 
		}
		print_test_result(1, name); 
		running_ref_counts[pg_num]--;
		exit();
	} else { // parent
		char* parents_shared_page = shmem_access(pg_num);
		running_ref_counts[pg_num]++;
		strcpy(parents_shared_page, test_str);
		wait();
	}
}

void process_gets_access_then_forks(){
	char* name = "process_gets_access_then_forks";
	char* test_str = "Bob\n";
	int pg_num = 1; 
	
	
	char* shared_page = shmem_access(pg_num);
	running_ref_counts[pg_num]++;
	
	int pid = fork();
	running_ref_counts[pg_num]++;
	
	if (pid == 0){ // child
		sleep(200); // let parent go first
		if( shmem_count(pg_num) != running_ref_counts[pg_num] ){
			print_test_result(0, name);
			printf(stdout, "\tfirst case\n");
			return; 
		}
		if (strcmp(shared_page, test_str) != 0){ 
			print_test_result(0, name);
			return; 
		}
		print_test_result(1, name); 
		
		running_ref_counts[pg_num]--;
		exit();
	} else { // parent
		strcpy(shared_page, test_str);
		wait();
	}
}

void parent_of_dead_child_cannot_see_childs_writing_post_mortem(){
	// didn't get to this
	
	// code tries to free a page once it's reference count is zero
		// for example, assuming no other processes,
		// if a program forks, then the child gets access then write and exits
		// and the parent waits until the child has exited then get access and read
		// then the parent should find nothing. Because when the child died, 
		// the shared page would have a reference count of zero 
		// (assuming no other processs are sharing it) and so it should be gotten rid of
}

int
main(int argc, char *argv[])
{
	// failing test  these both had 2 as pgnum 
	//		ref_counts_after_process_exit 
	// 		process_gets_access_then_forks 
	// ie 232 
	// same bug for 131
  printf(stdout, "Starting proj 2 tests\n");
  //~ deref_null();
  
	//~ ref_counts_after_process_exits();
  two_processes_simultaneously__fork_is_irrelevant();
  //~ process_gets_access_then_forks();
  
  exit();
}
