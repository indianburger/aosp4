CC=gcc
CFLAGS	+= -g $(shell pkg-config --libs --cflags glib-2.0)
basic: rvm.o basic.o 
	
clean: 
	rm -f basic rvm.o basic.o  
cleanall: clean
	rm -rf store
