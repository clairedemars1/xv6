// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){ // child
      exec("sh", argv);
      printf(1, "init: exec sh failed\n"); // if someone takes the bash shell, execs it into their program, that program ends, wouldn't we get here? (that would be bad, b/c no failure had happened)
      
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid) // when the new program dies, it goes here. Q: why doesn't it run a bunch of times?
      printf(1, "zombie!\n");
  }
}
