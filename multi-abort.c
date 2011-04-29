/* multi-abort -- test that aborting works with multiple segments */
#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEGNAME0  "testseg1"
#define SEGNAME1  "testseg2"

#define OFFSET0  10
#define OFFSET1  100

#define GOOD_STRING "hello, world"
#define BAD_STRING "i am a rock"


int main(int argc, char **argv)
{
     rvm_t rvm;
     char* segs[2];
     trans_t trans;

     /* initialize */
     rvm = rvm_init("rvm_segments");

     rvm_destroy(rvm, SEGNAME0);
     rvm_destroy(rvm, SEGNAME1);

     segs[0] = (char*) rvm_map(rvm, SEGNAME0, 1000);
     segs[1] = (char*) rvm_map(rvm, SEGNAME1, 1000);


     /* write in some initial data */
     trans = rvm_begin_trans(rvm, 2, (void **) segs);

     rvm_about_to_modify(trans, segs[0], OFFSET0, 100);
     strcpy(segs[0]+OFFSET0, GOOD_STRING);
     rvm_about_to_modify(trans, segs[1], OFFSET1, 100);
     strcpy(segs[1]+OFFSET1, GOOD_STRING);

     rvm_commit_trans(trans);

     
     /* start writing in some new data, but abort */
     trans = rvm_begin_trans(rvm, 2, (void **) segs);

     rvm_about_to_modify(trans, segs[0], OFFSET0, 100);
     strcpy(segs[0]+OFFSET0, BAD_STRING);
     rvm_about_to_modify(trans, segs[1], OFFSET1, 100);
     strcpy(segs[1]+OFFSET1, BAD_STRING);

     rvm_abort_trans(trans);
     

     /* test the strings */
     if(strcmp(segs[0] + OFFSET0, GOOD_STRING)) {
	  printf("ERROR in segment 0 (%s)\n",
		 segs[0]+OFFSET0);
	  exit(2);
     }
     if(strcmp(segs[1] + OFFSET1, GOOD_STRING)) {
	  printf("ERROR in segment 1 (%s)\n",
		 segs[1]+OFFSET1);
	  exit(2);
     }

     printf("OK\n");
     return 0;
}
