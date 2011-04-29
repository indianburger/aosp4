CC = gcc
RM = -rm -rf
AR = ar
CFLAGS = -Wall
targets = librvm.a objects tests

all: $(targets)

clean:
	$(RM) *.o $(targets) 
	$(RM) basic abort multi truncate multi-abort mid_trans_abort bad_unmap two_directories seg_expand
cleanall: clean
	rm -rf rvm_segments*
	
objects: rvm.c 
	gcc -g -c rvm.c `pkg-config --libs --cflags glib-2.0`

librvm.a: objects
	$(AR) rcs $@ *.o
	$(RM) *.o

tests: librvm.a
	gcc -o basic basic.c `pkg-config --libs --cflags glib-2.0` ./librvm.a
	gcc -o abort abort.c `pkg-config --libs --cflags glib-2.0` ./librvm.a	
	gcc -o multi multi.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o truncate truncate.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o multi-abort multi-abort.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o mid_trans_abort mid_trans_abort.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o bad_unmap bad_unmap.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o two_directories two_directories.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  
	gcc -o seg_expand seg_expand.c `pkg-config --libs --cflags glib-2.0` ./librvm.a  

