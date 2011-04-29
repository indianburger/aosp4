CC = gcc
RM = -rm -rf
AR = ar
CFLAGS = -Wall
targets = librvm.a objects tests

all: $(targets)

clean:
	$(RM) *.o $(targets) 
	$(RM) basic abort multi truncate
cleanall: clean
	rm -rf rvm_segments
	
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

