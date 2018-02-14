#include "types.h"
#include "user.h"
#include <stdint.h>

int stdout = 1;
//~ int running_ref_counts[4]; // stupid goober is not shared with child processes

void print_test_result(int passed, char* name){
	if (passed){
		printf(stdout, "Passed %s\n", name);
	} else {
		printf(stdout, "\tFAILED %s\n", name);
	}
}

void print_kernal_ref_counts(){
	printf(1, "%d %d %d %d\n", shmem_count(0), shmem_count(1), shmem_count(2), shmem_count(3));
}

void deref_null(){
	
	char* ptr = 0;
	
	printf(stdout, "now");
	*ptr = 'a'; // trap
	//~ printf(stdout, ptr); // trap
	//~ char t = *ptr; // linux trap, no trap in my code
	//~ t++;
}

void ref_counts_after_process_exits(){
	// starting ref counts 0 0 0 0 
	// exiting ref counts  0 0 0 0
	char* name = "ref_counts_after_process_exits";
	int pg_num = 1; // change back to 1
	int pid = fork();
	if (pid == 0){ // child // 0 1 0 0
		shmem_access(pg_num);
		if (shmem_count(pg_num) != 1 ){
			print_test_result(0, name);
		}
		/*int pid2 = fork();
		if (pid2 == 0){ // child
			
			printf(stdout, "exit grandchild\n");
			exit();
		} else {
			wait();
		}*/
		exit();
	} else {
		wait(); // let child go first
		
		if( shmem_count(pg_num) != 0){ 
			print_test_result(0, name);
			return; 
		}
		print_test_result(1, name);
	}
}

void two_processes_simultaneously__fork_is_irrelevant(){
	// starting reference counts 0 0 0 0
	// exit reference counts		0 0 0 1
	char* name = "two_processes_simultaneously__fork_is_irrelevant";
	char* test_str = "Ann\n";
	int pg_num = 3;
	int pid = fork();
	if (pid == 0){ // child
		sleep(200); // let parent go first
		
		
		char* childs_shared_page = shmem_access(pg_num);
		if( shmem_count(pg_num) != 2){ // 0 0 0 2
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
		
		exit();
	} else { // parent
		
		char* parents_shared_page = shmem_access(pg_num);
		strcpy(parents_shared_page, test_str);
		
		// before child is born
		if( shmem_count(pg_num) != 1){ // 0 0 0 1
			print_test_result(0, name);
			return; 
		}
		
		wait();
	}
}

void process_gets_access_then_forks(){
	// starting reference counts	0 0 0 1
	// exit 						0 1 0 0 
	char* name = "process_gets_access_then_forks";
	char* test_str = "Bob\n";
	int pg_num = 1; 
	
	if( shmem_count(pg_num) != 0 ){
			print_test_result(0, name);
			printf(stdout, "ref count %d\n", shmem_count(pg_num));
			return; 
	}
	char* shared_page = shmem_access(pg_num);
	// 0 1 0 0 
	if( shmem_count(pg_num) != 1 ){
			print_test_result(0, name);
			printf(stdout, "ref count %d\n", shmem_count(pg_num));
			return; 
	}
	int pid = fork(); 
	
	if (pid == 0){ // child
		// 0 2 0 0
		sleep(200); // let parent go first
		if( shmem_count(pg_num) != 2 ){
			print_test_result(0, name);
			printf(stdout, "ref count %d\n", shmem_count(pg_num));
			return; 
		}
		if (strcmp(shared_page, test_str) != 0){ 
			print_test_result(0, name);
			return; 
		}
		print_test_result(1, name); 
		
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
  
	ref_counts_after_process_exits();
    two_processes_simultaneously__fork_is_irrelevant();
	process_gets_access_then_forks();
  
  exit();
}
