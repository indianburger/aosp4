#include "rvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

//utility functions
void verbose_print(const char* msg){
    if (verbose_debug){
        fprintf(stderr, "\n%s", msg);
    }
}

//Function returns 0 if file does not exist,
//non-zero otherwise.
int file_exists(const char* file){
    struct stat statbuf;
    int f_desc = open(file, O_RDONLY);
    return !fstat (f_desc, &statbuf); 

}

seg_t* find_seg (void* segbase){
    GSList* iterator = mapped_seg_list;
    for (; iterator; iterator = iterator->next) {
        seg_t* seg = (seg_t*)(iterator -> data);
        if (seg -> mem == segbase){
            return seg;
        }
    }
    return NULL;
}
//rvm functions
rvm_t rvm_init(const char* directory){
    int temp;
    rvm_t rvm;
    char log_path[FILE_NAME_LEN];
    snprintf(log_path, FILE_NAME_LEN, "%s/%s", directory, LOG_NAME);
    if (!file_exists(directory)){
        //directory does not exist
        temp = mkdir(directory, S_IRWXU);
        if (temp){
            perror("\nDirectory creation error");
            exit(EXIT_FAILURE);
        }
        {
            char temp_str[MSG_LEN];
            snprintf(temp_str, MSG_LEN, "\nrvm_init: Created directory at: %s", directory);
            verbose_print(temp_str);
        }
        if (fopen(log_path, "w") == NULL){
            perror("\nLog file create error");
        }
    }
    else{
        verbose_print("\nrvm_init: Using existing directory");
        if (file_exists(log_path)){
            rvm.log_path = log_path;
        }
        else{
            fprintf(stderr, "\nExisting directory does not have log file.");
            abort();
        }
    }
    rvm.dir_path = directory;
    mapped_seg_list = NULL;
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
    int x;
    char file_path[50];
    FILE *file;
    void *buffer;
    unsigned long file_len;
    struct stat st;
    snprintf(file_path, 50, "./%s/%s.seg", rvm.dir_path, segname);
    if (file_exists(file_path)){
        //read contents in backing store to memory

        file_len = file_size(file_path);
        fprintf(stderr, "\noriginal size: %ld", file_len);
        fd = open(file_path, O_RDWR , (mode_t)0600);
        if (file_len < size_to_create){
            //Seek to one byte before required size and write 
            //one byte so that file is extended

            if (!fd)
            {
                perror("Error opening existing segment file");
                exit(EXIT_FAILURE);
            }
            result = lseek(fd, 0, SEEK_SET);
            while ((result = read(fd, &x, sizeof(int))) > 0){
                fprintf(stderr, "\nOriginal read: %d", x);

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
            fprintf(stderr, "\nnew size: %ld", file_len);
            
            //close(fd);
        }
	           
	    ////Allocate memory
	    //buffer = malloc(size_to_create);
	    //if (!buffer)
	    //{
        //    perror("\nError allocating memory");
        //    fclose(file);
        //    exit(EXIT_FAILURE);
	    //}

	    ////Read file contents into buffer
	    //file = fopen(file_path, "rb");
	    //fseek(file, 0, SEEK_SET);
	    //result = fread(buffer, size_to_create, 1, file);
        //printf("successful read of %d bytes", result);
	    //fclose(file);

        fprintf(stderr, "\nRead contents from segment at %s of size %d", file_path, size_to_create);
    }
    else{
        //no previous segment. So nothing to read. Just create a file
        //of required size and create an empty buffer to return.
        //Creating an empty file of required size may not be necessary later.
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
        fprintf(stderr, "\nCreated segment %s in %s", segname, file_path);
        //close(fd);

        //create an empty buffer
	    //buffer = calloc(1, size_to_create);
    }
    buffer = mmap (0, size_to_create, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    seg_t* segment = (seg_t*)malloc(sizeof(seg_t));
    segment -> name = segname;
    segment -> size = size_to_create;
    segment -> mem = buffer;

    mapped_seg_list = g_slist_append(mapped_seg_list, segment);
    
    fprintf(stderr, "\nRVM list size after rvm_map: %d", g_slist_length(mapped_seg_list));
    return buffer;

}

void rvm_unmap(rvm_t rvm, void* segbase){
    seg_t* seg = find_seg(segbase);
    if (seg == NULL){
        fprintf(stderr, "\nSegment not found. Cannot be unmapped.");
        abort();
    }
    if (munmap(segbase, seg -> size) == -1) { 
	    perror("Error un-mmapping the file");
    }
    else{
        verbose_print("\nUnmapped segment successfully. ");
    }
    //TODO: need to close file also
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases){
    GList* keys;
    int tid, i;

    if (trans_mapping == NULL){
        tid = 1;
        trans_mapping = g_hash_table_new(g_int_hash, g_int_equal);
    }
    else{
        keys = g_hash_table_get_keys (trans_mapping);
        tid = g_list_length(keys) + 1; 
    }
    
    trans_info_t* info = (trans_info_t*)malloc(sizeof(trans_info_t));
    info -> trans_seg_list = NULL;
    info -> old_value_items = NULL;
    for (i = 0; i < numsegs; i++){
        seg_t* seg = find_seg(segbases[i]);
        if (seg == NULL){
            fprintf(stderr, "\nError. rvm_begin_trans has a segbase that is not mapped");
            abort();
        }
        char temp_str[MSG_LEN];
        snprintf(temp_str, MSG_LEN, "\nrvm_begin_trans: Adding %s segment to transaction", seg -> name);
        verbose_print(temp_str);
        info -> trans_seg_list = g_slist_append(
                info -> trans_seg_list, seg);

    }
    int* tid_cpy = (int*)malloc(sizeof(int));
    *tid_cpy = tid;
    printf ( "Insertio tid %d\n", *tid_cpy );
    g_hash_table_insert(trans_mapping, tid_cpy, info); 
    printf ( "hash size: %d\n", g_hash_table_size(trans_mapping));
    return tid; 
}

void rvm_about_to_modify(trans_t tid, void* segbase, int offset, int size){
    seg_t* seg = find_seg(segbase);
    trans_item_t* item = (trans_item_t*)malloc(sizeof(trans_item_t));
    
    if (seg == NULL){
        fprintf(stderr, "\nrvm_about_to_modify: segment not found for given tid. ");
        abort();
    }
    item -> seg = seg;
    item -> offset = offset;
    item -> size = size;
    item -> old_value = malloc(size);
    memcpy(item -> old_value, segbase + offset, size);

    trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
    if (info -> old_value_items == NULL){
        info -> old_value_items = g_queue_new();

        trans_info_t* item = NULL;
        g_queue_push_tail(info -> old_value_items, item);
    }
    g_queue_push_tail(info -> old_value_items, item);
    fprintf(stderr, "\naBOUTING: segname: %s, offset: %d, size: %d", seg-> name, item -> offset, item -> size);
    

    //trans_item_t* item1 = (trans_item_t*) g_queue_pop_tail(info -> old_value_items);
    //seg_t* seg1 = item1 -> seg;
    //fprintf(stderr, "\nREMOVING IMM: segname: %s, offset: %d, size: %d", seg1-> name, item1 -> offset, item1 -> size);
}

void rvm_commit_trans(trans_t tid){
    trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
    GSList* seg_list = info -> trans_seg_list;
    seg_t* seg;
    GSList* iterator;
    FILE* log_file;
    log_file = fopen("store/txn.log", "a");

    for(iterator = seg_list; iterator; iterator = iterator -> next){
        seg = (seg_t*)iterator -> data;
        fprintf(log_file, "%s~", seg -> name);
        fwrite(seg -> mem, seg -> size, 1, log_file);
        fputs("\n", log_file);
    }

    fclose(log_file);

}
void rvm_abort_trans(trans_t tid){
    trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
    //GQueue* old_values = info -> old_value_items;

    while((info -> old_value_items) != NULL && g_queue_get_length(info -> old_value_items)){
        //read list of old value items backwards and apply them
        //on the segments in memory for all items for this tid.
        trans_item_t* item = (trans_item_t*) g_queue_pop_tail(info -> old_value_items);
        if (item == NULL) {

            continue;
        };
        seg_t* seg = item -> seg;
        
        
        fprintf(stderr, "\naborting: segname: %s, offset: %d, size: %d", seg-> name, item -> offset, item -> size);
        memcpy(seg + item -> offset, item -> old_value, item -> size);


    }
    
    verbose_print("\nrvm_abort_trans: Successfull abort");
}


void rvm_truncate_log(rvm_t rvm){

}

int main(){
    char* buffers[3];
    int i;
    GSList* iterator = NULL;
    rvm_t rvm = rvm_init("store");
    fprintf(stderr, "\ncreated path: %s", rvm.dir_path);
    buffers[0] = (char*)rvm_map(rvm, "seg0", 10);
    buffers[1] = (char*)rvm_map(rvm, "seg1", 10);
    buffers[2] = (char*)rvm_map(rvm, "seg2", 10);

    snprintf(buffers[0]+1, 10, "BLAH1");
    snprintf(buffers[1]+2, 10, "BLAH2");
    snprintf(buffers[2]+3, 10, "BLAH3");
    
    fprintf(stderr, "\nBEFORE\n\n");
    fprintf(stderr, "\nbuffer[0]:\n");
    fwrite(buffers[0], 10, 1, stderr);
    fflush(stderr);
    fprintf(stderr, "\nbuffer[1]:\n");
    fwrite(buffers[1], 10, 1, stderr);
    fflush(stderr);

    fprintf(stderr, "\nbuffer[2]:\n");
    fwrite(buffers[2], 10, 1, stderr);
    fflush(stderr);
    
    
    trans_t tid = rvm_begin_trans(rvm, 3, (void**)buffers);
    fprintf(stderr, "\ntid: %d ", tid);
    

    //rvm_about_to_modify(tid, buffers[0], 1, 6);
    snprintf(buffers[0]+1, 10, "TEST1");
    rvm_about_to_modify(tid, buffers[1], 2, 6);
    snprintf(buffers[1]+2, 10, "TEST2");
    
    rvm_about_to_modify(tid, buffers[2], 3, 6);
    snprintf(buffers[2]+3, 10, "TEST3");


    //rvm_commit_trans(tid);
    rvm_abort_trans(tid);
    fprintf(stderr, "\nAFTER\n\n");
    fprintf(stderr, "\nbuffer[0]:\n");
    fwrite(buffers[0], 10, 1, stderr);
    fflush(stderr);
    fprintf(stderr, "\nbuffer[1]:\n");
    fwrite(buffers[1], 10, 1, stderr);
    fflush(stderr);

    fprintf(stderr, "\nbuffer[2]:\n");
    fwrite(buffers[2], 10, 1, stderr);
    fflush(stderr);
    rvm_unmap(rvm, (void*)buffers[0]); 
    rvm_unmap(rvm, (void*)buffers[1]);
    rvm_unmap(rvm, (void*)buffers[2]);
    
    fprintf(stderr, "\n\n");
    abort();
}
