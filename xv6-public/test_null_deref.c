#include "types.h"
#include "user.h"
#include <stdint.h>

int stdout = 1;

void deref_null(){
	char* ptr = 0;
	*ptr = 'a'; // traps
	
	//~ printf(stdout, ptr); // traps
	
	//~ char t = *ptr; // linux traps, mine doesn't
	//~ t++;
}

int
main(int argc, char *argv[])
{
  printf(stdout, "Starting proj 2 tests\n");
  deref_null();
  
  exit();
}
