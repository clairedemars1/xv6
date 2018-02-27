#include "types.h"
#include "user.h"
//~ #include "kthreads.h"

#define NULL 0

void run_me(void* arg){
	printf(1, "run_me sucessfully ran\n");
}

int
main(int argc, char *argv[])
{
	
	int thread = clone(run_me, NULL, NULL);
	thread++;
	printf(1, "I did not crash\n");
	exit();
}
