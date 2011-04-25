#include "rvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int file_exists(const char* file){
    struct stat statbuf;
    int f_desc = open(file, O_RDONLY);
    return !fstat (f_desc, &statbuf); 

}
rvm_t rvm_init(const char* directory){
    int temp;
    rvm_t rvm;
    if (!file_exists(directory)){
        //directory does not exist
        temp = mkdir(directory, S_IRWXU);
        if (temp){
            perror("\nDirectory creation error");
            exit(EXIT_FAILURE);
        }
        printf("\nrvm_init: Created directory at: %s", directory);
    }
    else{
        printf("\nrvm_init: Using existing directory");
    }
    rvm.dir_path = directory;
    return rvm;
}

unsigned long file_size(const char* file_path){
    struct stat st;
    int result = stat(file_path, &st);

    if (result){
        perror("Error reading file stat");
        exit(EXIT_FAILURE);
    }
    return st.st_size;
    
}
void* rvm_map(rvm_t rvm, const char* segname, int size_to_create){
    int fd, result;
    char file_path[50];
    FILE *file;
    void *buffer;
    unsigned long file_len;
    struct stat st;
    snprintf(file_path, 50, "./%s/%s.seg", rvm.dir_path, segname);
    if (file_exists(file_path)){
        //read contents in backing store to memory

        file_len = file_size(file_path);
        printf("\noriginal size: %ld", file_len);
        if (file_len < size_to_create){
            //Seek to one byte before required size and write 
            //one byte so that file is extended
            fd = open(file_path, O_RDWR  , (mode_t)0600);

            if (!fd)
            {
                perror("Error opening existing segment file");
                exit(EXIT_FAILURE);
            }
            result = lseek(fd, size_to_create - 1, SEEK_SET);
            if (!result){
                close(fd);
                perror("\nlseek error while stretching file");
                exit(EXIT_FAILURE);
            }

            result = write(fd, "", 1);
            if (!result){
                perror("\nError writing last byte");
                exit(EXIT_FAILURE);
            }
     
            file_len = file_size(file_path);
            printf("\nnew size: %ld", file_len);
            
            close(fd);
        }
	           
	    //Allocate memory
	    buffer = malloc(size_to_create);
	    if (!buffer)
	    {
            perror("\nError allocating memory");
            fclose(file);
            exit(EXIT_FAILURE);
	    }

	    //Read file contents into buffer
	    file = fopen(file_path, "rb");
	    fseek(file, 0, SEEK_SET);
	    result = fread(buffer, size_to_create, 1, file);
        printf("successful read of %d bytes", result);
	    fclose(file);
        printf("\nRead contents from segment at %s of size %ld", file_path, file_len);
    }
    else{
        fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
        if (!fd){
            perror("\nSegment file creation error");
            exit(EXIT_FAILURE);
        }
        result = lseek(fd, size_to_create - 1, SEEK_SET);
        if (!result){
            close(fd);
            perror("\nlseek error while stretching file");
            exit(EXIT_FAILURE);
        }

        result = write(fd, "", 1);
        if (!result){
            perror("\nError writing last byte");
            exit(EXIT_FAILURE);
        }
        printf("\nCreated segment %s in %s", segname, file_path);
        close(fd);

        //create an empty buffer
	    buffer = calloc(1, size_to_create);
    }
    return buffer;

}

void rvm_unmap(rvm_t rvm, void* segname){

}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases){

}

void rvm_about_to_modify(trans_t tid, void* segbase, int offset, int size){

}

void rvm_commit_trans(trans_t tid){

}
void rvm_abort_trans(trans_t tid){

}

int main(){
    void* buffer;
    rvm_t rvm = rvm_init("store");
    printf("\ncreated path: %s", rvm.dir_path);
    buffer = rvm_map(rvm, "seg1", 50);
    printf("Contents in segment: %s <END>", (char*)buffer);
    printf("\n\n");
}
