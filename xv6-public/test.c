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
		int pid2 = fork();
		if (pid2 == 0){ // grandchild
			if (shmem_count(pg_num) != 2 ){
				print_test_result(0, name);
			}
			exit();
		} else {
			wait();
			if (shmem_count(pg_num) != 1 ){
				print_test_result(0, name);
			}
		} 
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

void two_processes_get_access_after_fork(){
	// starting reference counts 0 0 0 0
	// exit reference counts		0 0 0 1
	char* name = "two_processes_get_access_after_fork";
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
	// exit 						0 1 0 1 
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




void basic_ref_counts(){
	// starting 0101
	// ending	0101 (b/c requests on 0, 1)
	int passed = 1; 
	char* ret;
	char* name = "basic_ref_counts";
		
	// invalid page number requests
	ret = shmem_access(4);
	if (ret != 0) passed = 0;
	ret = shmem_access(-1); 
	if (ret != 0) passed = 0;
	int count = shmem_count(4);
	if (count != -1) passed = 0;
	
	int pid = fork(); // 0202
	if (pid == 0) {
		print_kernal_ref_counts();
			
		// show that pg_num == 0 does not cause weird zeroey stuff
		shmem_access(0); // 1202
		
		//check it's right
		print_kernal_ref_counts();
		if (shmem_count(0) != 1
			|| shmem_count(1) != 2
			|| shmem_count(2) != 0
			|| shmem_count(3) != 2){
			passed = 0;
		}
		print_test_result(passed, name);	
		
		exit();

	} else {
		wait();
		print_kernal_ref_counts(); //0101
	}
}


void parent_of_dead_child_cannot_see_childs_writing_post_mortem(){
	// this is not really a test, more of a check for implementation bugs
	// (which themselves are only bugs if I commit to a specific interpretation of something 
	// that was left ambiguous in the assignment)
	// basically it's just here b/c I didn't realize it wasn't very helpful until I spent time on it
	
	// parent waits for child, child writes then dies
	// parent shouold not expect to be able to read what child wrote
	
	char* test_str = "Ca";
	char* name = "parent_of_dead_child_cannot_see_childs_writing_post_mortem";
	// starting 0101
	// ending 1101
		
	int pid = fork();
	if (pid == 0){
		char* pg = shmem_access(0);
		strcpy(pg, test_str);
		exit();
	}else {
		wait();
		char* pg = shmem_access(0);
		if (pg[0] == 'C' && pg[1] == 'a'){
			printf(1, "Uncertain result: Could indicate a bug, but might just be same page being mapped to new va\n");
		} else {
			print_test_result(1, name);
		}
	}
}

int
main(int argc, char *argv[])
{
	// function 0 and 3 use same page number, which tests process's shared_pages initialization (I know b/c it used to have bug)
	ref_counts_after_process_exits(); // pg num 1
    two_processes_get_access_after_fork(); // pg num 3
	process_gets_access_then_forks(); // pg num 1
	basic_ref_counts();  // tests bad arguments to sys calls
	parent_of_dead_child_cannot_see_childs_writing_post_mortem(); // not really a test
  
  exit();
}
