#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "procinfo.h" 

int stdout = 1;

void getprocsinfotest(){
	
	struct procinfo* procs = 0;
	int proc_count = getprocsinfo(procs);
	
	if(proc_count != 3){ printf(stdout, "TEST FAILED"); exit(); }
	int i;
	for (i=0; i<proc_count; i++){
		printf(stdout, "pid: %d, name: %s\n", procs[i].pid, procs[i].name);
	}
	
	int new_proc_pid = fork();
	if(new_proc_pid < 0){
		printf(stdout, "fork failed\n");
		exit();
	}
	proc_count = getprocsinfo(procs);
	if(proc_count != 4){ printf(stdout, "TEST FAILED"); exit(); }
	for (i=0; i<proc_count; i++){
		printf(stdout, "pid: %d, name: %s\n", procs[i].pid, procs[i].name);
	}

	printf(stdout, "getprocsinfotest passed\n");
	
}

int
main(int argc, char *argv[])
{
  printf(1, "usertests starting\n");
  getprocsinfotest();
  exit();
}
