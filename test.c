#include <stdio.h>

struct s {
	int field;
	int* inner_p;
};
 
int main(){
	
	struct s my_struct;
	my_struct.field = 3;
	int i = 4;
	my_struct.inner_p = &i;
	printf("field: %d\n", my_struct.field);
	struct s* p = &my_struct;
	struct s** p_to_p = &p;
	//~ printf("field again : %d\n", p->field);
	printf("field again: %d\n", (*p_to_p)->field);
	//~ printf("unknown: %d\n", *p_to_p->field); // does not compile, thus * does not happen before ->, thus -> happens first
	
	printf("should be same: %d %d\n", *p->inner_p,*(p->inner_p) ); // confirmation: -> happens first 
	//~ printf("should be 4: %d\n", *p->inner_p); 

	
	
	
	return 0;

	
}
