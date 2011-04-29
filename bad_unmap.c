/*bad_unmap.c Unmaps a segment before end of transaction and should throw error */
#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_STRING1 "hello, world"

int main(int argc, char **argv)
{
     rvm_t rvm;
     char *seg;
     void *segs[1];
     trans_t trans;
     
     rvm = rvm_init("rvm_segments");
     
     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     seg = (char *) segs[0];

     fprintf(stderr, "\nThere should be an error below that segment cannot be unmapped\n");
     trans = rvm_begin_trans(rvm, 1, segs);
     rvm_about_to_modify(trans, seg, 0, 100);
     sprintf(seg, TEST_STRING1);
     
     rvm_unmap(rvm, (void*)trans); 
     
     rvm_commit_trans(trans);

     exit(0);
}

