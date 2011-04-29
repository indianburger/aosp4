/* basic.c - test that basic persistency works */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define FIRST_STRING "hello first portion"
#define SECOND_STRING "hello second portion"
#define OFFSET 100
/* proc1 writes some data, commits it, then exits */
void proc1() 
{
     rvm_t rvm;
     trans_t trans;
     char* segs[0];
     
     rvm = rvm_init("rvm_segments");
     rvm_destroy(rvm, "testseg"); 
     segs[0] = (char *) rvm_map(rvm, "testseg", 1000);

     
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     
     rvm_about_to_modify(trans, segs[0], 0, 30);
     sprintf(segs[0], FIRST_STRING);
     rvm_commit_trans(trans);
     rvm_unmap(rvm, segs[0]);

     fprintf(stderr, "\nCheck size of segment BEFORE expanding\n");
     system("ls -l rvm_segments/testseg.seg");
     
     segs[0] = (char *) rvm_map(rvm, "testseg", 200);    
     trans = rvm_begin_trans(rvm, 1, (void **) segs);
     rvm_about_to_modify(trans, segs[0], OFFSET, 30);
     sprintf(segs[0]+OFFSET, SECOND_STRING);
     rvm_commit_trans(trans);
     fprintf(stderr, "\nCheck size of segment AFTER expanding\n");
     system("ls -l rvm_segments/testseg.seg");
     abort();
}


/* proc2 opens the segments and reads from them */
void proc2() 
{
     char* segs[1];
     rvm_t rvm;
     
     rvm = rvm_init("rvm_segments");
     fprintf(stderr, "\nReading from expanded section of segment");
     segs[0] = (char *) rvm_map(rvm, "testseg", 200);
     //fprintf(stderr, "\nCOMPARING %s and %s", segs[0] + OFFSET, SECOND_STRING);
     if(strcmp(segs[0]+OFFSET, SECOND_STRING)) {
	  printf("ERROR: second hello not present\n");
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
