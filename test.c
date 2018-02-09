#include <stdio.h>


void testmemproj2(){
	char* ptr = 0;
	*ptr = 3; // gives seg fault
	//~ printf("val at ptr: %d\n", *ptr); // gives seg fault
}


int
main(int argc, char *argv[])
{
  printf("usertests starting\n");
  testmemproj2();
}
