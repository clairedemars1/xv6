#include "types.h"
#include "user.h"

int stdout = 1;

void testmemproj2(){
	char* ptr = 0;
	printf(stdout, "val at ptr: %d\n", *ptr);
}


int
main(int argc, char *argv[])
{
  printf(stdout, "usertests starting\n");
  testmemproj2();
  exit();
}
