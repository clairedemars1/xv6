#include <stdlib.h>

int main(){
	
	char* ptr = 0;
	//*ptr; // no linux trap
	
	*ptr = 'a'; // both trap
	
	//~ char t = *ptr; // linux trap, no trap in my code
	//~ t++;
	
	
	
	return 0;
}
