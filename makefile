compile: basic.c rvm.h rvm.c 
	gcc -o rvm rvm.h
	gcc -o basic basic.c

	
clean: 
	rm -f rvm basic 
cleanall: clean
	rm -rf store
