/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define FIRST_STRING "FIRST DIRECTORY"
#define SECOND_STRING "SECOND DIRECTORY"

/* proc1 writes some data, commits it, then exits */
void proc1() 
{
     rvm_t rvm;
     trans_t trans;
     char* segs[0];
     
     fprintf(stderr, "\nCreating FIRST directory");
     rvm = rvm_init("rvm_segments1");
     rvm_destroy(rvm, "testseg"); 
     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], 0, 100);
     sprintf(segs[0], FIRST_STRING);
     
     rvm_commit_trans(trans);
     fprintf(stderr, "\nCreating SECOND directory");
     rvm = rvm_init("rvm_segments2");
     rvm_destroy(rvm, "testseg"); 
     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], 0, 100);
     sprintf(segs[0], SECOND_STRING);
     
     rvm_commit_trans(trans);

     abort();
}


/* proc2 opens the segments and reads from them */
void proc2() 
{
     char* segs[1];
     rvm_t rvm;
     
     rvm = rvm_init("rvm_segments1");

     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     if(strcmp(segs[0], FIRST_STRING)) {
	  printf("ERROR: first directory string not present\n");
	  exit(2);
     }
     rvm = rvm_init("rvm_segments2");

     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     if(strcmp(segs[0], SECOND_STRING)) {
	  printf("ERROR: second directory not present\n");
	  exit(2);
     }

     printf("OK\n");
     exit(0);
}


int main(int argc, char **argv)
{
     int pid;

     pid = fork();
     if(pid < 0) {
	  perror("fork");
	  exit(2);
     }
     if(pid == 0) {
	  proc1();
	  exit(0);
     }

     waitpid(pid, NULL, 0);

     proc2();

     return 0;
}
