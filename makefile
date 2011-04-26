compile: rvm.h rvm.c 
	gcc -o rvm rvm.c `pkg-config --libs --cflags glib-2.0`
	
clean: 
	rm -f rvm  
cleanall: clean
	rm -rf store
