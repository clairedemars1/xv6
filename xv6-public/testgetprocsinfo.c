#include "types.h"
#include "user.h"
#include "syscall.h"
#include "procinfo.h" 

int stdout = 1;

void getprocsinfotest(){
	
	struct procinfo* procs = 0;
	
	// originally
	int proc_count = getprocsinfo(procs);
	getprocsinfo(procs);

	if(proc_count != 3){ printf(stdout, "TEST FAILED: not 3 processes "); exit(); }
	int i;
	printf(stdout, "\nbefore fork\n");
	for (i=0; i<proc_count; i++){
		printf(stdout, "pid: %d, name: %s\n", procs[i].pid, procs[i].name);
	}

	// after a fork
	int new_proc_pid = fork();
	if(new_proc_pid < 0){
		printf(stdout, "fork failed\n");
		exit();
	} else if (new_proc_pid == 0){
		// child
		proc_count = getprocsinfo(procs);
		printf(stdout, "\nafter fork\n");
		if(proc_count != 4){ printf(stdout, "child failture"); exit(); }
		for (i=0; i<proc_count; i++){
			printf(stdout, "pid: %d, name: %s\n", procs[i].pid, procs[i].name);
		}
		printf(stdout, "getprocsinfotest passed for child\n");
		printf(stdout, "TESTS PASSED\n");

	} else {
		// parent
		int proc_count = getprocsinfo(procs);
		if(proc_count != 4){ printf(stdout, "parent failure: "); exit(); }
		printf(stdout, "getprocsinfotest passed for parent\n");
		wait();
	}
}


void call_user_version_test(){
	call_user_version();
}

int
main(int argc, char *argv[])
{
  printf(1, "usertests starting\n");
  getprocsinfotest();
  //~ call_user_version_test();
  exit();
}
