#include "rvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

int verbose_debug = 1;
GList* mapped_seg_list = NULL; //list of seg_t objects
GHashTable* trans_mapping = NULL; //maps trans_t to trans_info object 
#define MAX_SEG_SIZE 100000

//utility functions
void verbose_print(const char* msg) {
	if (verbose_debug) {
		fprintf(stderr, "\n%s", msg);
	}
}

//Function returns 0 if file does not exist,
//non-zero otherwise.
int file_exists(const char* file) {
	struct stat statbuf;
	int f_desc = open(file, O_RDONLY);
	int status;
	status = !fstat(f_desc, &statbuf);
	//if(status)
	close(f_desc);
	//fprintf(stderr, "\nfile status =  %d\n", f_desc);
	return status;
}

seg_t* find_seg(void* segbase) {
	GList* iterator = mapped_seg_list;
	for (; iterator; iterator = iterator->next) {
		seg_t* seg = (seg_t*) (iterator -> data);
		if (seg -> mem == segbase) {
			return seg;
		}
	}
	return NULL;
}
//rvm functions
rvm_t rvm_init(const char* directory) {
	int temp;
	rvm_t rvm;
	char* log_path = (char *) malloc(FILE_NAME_LEN);
	snprintf(log_path, FILE_NAME_LEN, "%s/%s", directory, LOG_NAME);
	if (!file_exists(directory)) {
		//directory does not exist
		temp = mkdir(directory, S_IRWXU);
		if (temp) {
			perror("\nDirectory creation error");
			exit(EXIT_FAILURE);
		}
		{
			char temp_str[MSG_LEN];
			snprintf(temp_str, MSG_LEN, "\nrvm_init: Created directory at: %s",
					directory);
			verbose_print(temp_str);
		}
		if (fopen(log_path, "w") == NULL) {
			perror("\nLog file create error");
		}
	} else {
		verbose_print("\nrvm_init: Using existing directory");
		if (file_exists(log_path)) {
			//rvm.log_path = log_path;
			//strcpy((char*)rvm.log_path, log_path);
			fprintf(stderr, "");
		} else {
			fprintf(stderr, "\nExisting directory does not have log file.");
			abort();
		}
	}
	rvm.dir_path = directory;
	mapped_seg_list = NULL;
	rvm.log_path = log_path;
	//strcpy((char *)rvm.log_path, log_path);
	fprintf(stderr, "\nthe log path of rvm is set to - %s\n", rvm.log_path);
	return rvm;
}

unsigned long file_size(const char* file_path) {
	struct stat st;
	int result = stat(file_path, &st);

	if (result) {
		perror("Error reading file stat");
		exit(EXIT_FAILURE);
	}
	return st.st_size;

}

void writeDataToDisk(rvm_t rvm, const char* segname, int trunc_flag) {
	FILE* seg_file;
	FILE* log_file;
	log_file = fopen(rvm.log_path, "r");
	if (log_file == NULL) {
		perror("writeDataToDisk: Error opening log_file");
		abort();
	}

	char ch;
	char seg_data[MAX_SEG_SIZE];
	char buffer[MAX_SEG_SIZE + FILE_NAME_LEN + 10];
	int seg_size;

	// read char by char through the log file
	// till you get to ~, thats the name of the segment
	// from there to the next tilde is the size of the memory segment
	// compare segname to the segname passed as argument
	// if match then read from buffer and
	// write that value in the seg_file

	while (fgets(buffer, MAX_SEG_SIZE + FILE_NAME_LEN + 10, log_file) != NULL) {
		int i = 0;
		char curr_segname[100];
		char curr_segsize[100];

		for (; buffer[i] != '~'; i++) {
			curr_segname[i] = buffer[i];
		}
		curr_segname[i] = '\0';
		fprintf(stderr, "\nSegname in log: %s", curr_segname);
		fprintf(stderr, "\ni: %d", i);
		if (!trunc_flag && strcmp(segname, curr_segname))
			continue;
		char curr_segname_full[FILE_NAME_LEN];
		snprintf(curr_segname_full, FILE_NAME_LEN, "%s/%s.seg", rvm.dir_path, curr_segname);
		fprintf(stderr, "\nSegname in log HERE: %s and dir: %s", curr_segname_full, rvm.dir_path);
		seg_file = fopen(curr_segname_full, "w");
		if (seg_file == NULL){
			perror("\nwriteDataToDisk: Error opening seg_file");
			abort();
		}
		int j = 0;
		i++;
		for (; buffer[i] != '~'; i++) {
			curr_segsize[j++] = buffer[i];
		}
		fprintf(stderr, "\ni: %d", i);
		curr_segsize[j] = '\0';
		seg_size = atoi(curr_segsize);
		fprintf(stderr, "\nSegsize in log: %d", seg_size);

		j = 0;
		for (++i; j < seg_size; j++, i++) {
			//fprintf(stderr, "\ntada:%c!", buffer[i]);
			fputc(buffer[i], seg_file);
		}
		fprintf(stderr, "\ni: %d", i);
		fprintf(stderr, "\nThis is after the read %ld\n", ftell(log_file));
		

		fclose(seg_file);
	}
	
	fclose(log_file);
	if (trunc_flag){
		FILE* log_to_del = fopen(rvm.log_path, "w");
		fclose(log_to_del);
	}
}

void* rvm_map(rvm_t rvm, const char* segname, int size_to_create) {
	int fd, result;
	int x;
	char file_path[50];
	FILE *file;
	void *buffer;
	unsigned long file_len;
	struct stat st;
	snprintf(file_path, 50, "./%s/%s.seg", rvm.dir_path, segname);
	if (file_exists(file_path)) {
		writeDataToDisk(rvm, segname, 0); // method to write the data to disk from the txn logs

		//read contents in backing store to memory
		file_len = file_size(file_path);
		fprintf(stderr, "\noriginal size: %ld", file_len);
		fd = open(file_path, O_RDWR, (mode_t) 0600);

		if (file_len < size_to_create) {
			//Seek to one byte before required size and write
			//one byte so that file is extended

			if (!fd) {
				perror("Error opening existing segment file");
				exit(EXIT_FAILURE);
			}
			result = lseek(fd, 0, SEEK_SET);
			while ((result = read(fd, &x, sizeof(int))) > 0) {
				fprintf(stderr, "\nOriginal read: %d", x);

			}
			result = lseek(fd, size_to_create - 1, SEEK_SET);
			if (!result) {
				close(fd);
				perror("\nlseek error while stretching file");
				exit(EXIT_FAILURE);
			}

			result = write(fd, "", 1);
			if (!result) {
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

		fprintf(stderr, "\nRead contents from segment at %s of size %d",
				file_path, size_to_create);
		/* Added by Nadu for testing
		 adding to the mapped_seg_list was giving an error so I pushed this up and do not add to the queue
		 buffer = mmap (0, size_to_create, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		 seg_t* segment = (seg_t*)malloc(sizeof(struct seg));
		 segment->name = segname;
		 segment->size = size_to_create;
		 segment->mem = buffer;

		 fprintf(stderr,"\n is it able to append the segment? name = %s segment addr = %p\n", segment->name, buffer);
		 return buffer; */

	} else {
		//no previous segment. So nothing to read. Just create a file
		//of required size and create an empty buffer to return.
		//Creating an empty file of required size may not be necessary later.
		fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
		if (!fd) {
			perror("\nSegment file creation error");
			exit(EXIT_FAILURE);
		}
		result = lseek(fd, size_to_create - 1, SEEK_SET);
		if (!result) {
			close(fd);
			perror("\nlseek error while stretching file");
			exit(EXIT_FAILURE);
		}

		result = write(fd, "", 1);
		if (!result) {
			perror("\nError writing last byte");
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "\nCreated segment %s in %s", segname, file_path);
		//close(fd);

		//create an empty buffer
		//buffer = calloc(1, size_to_create);
	}

	buffer
			= mmap(0, size_to_create, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
					0);
	if (buffer == MAP_FAILED) {
		perror("\nrvm_map mmap failed ");
	}

	seg_t* segment = (seg_t*) malloc(sizeof(struct seg));
	segment->name = segname;
	segment->size = size_to_create;
	segment->mem = buffer;

	fprintf(
			stderr,
			"\n In rvm_map is it able to append the segment? name = %s segment addr = %p\n",
			segment->name, buffer);

	mapped_seg_list = g_list_append(mapped_seg_list, segment);
	fprintf(stderr, "\nRVM list size after rvm_map: %d \n", g_list_length(
			mapped_seg_list));

	return buffer;

}

void rvm_unmap(rvm_t rvm, void* segbase) {
	seg_t* seg = find_seg(segbase);
	if (seg == NULL) {
		fprintf(stderr, "\nSegment not found. Cannot be unmapped.");
		abort();
	}
	if (munmap(segbase, seg -> size) == -1) {
		perror("Error un-mmapping the file");
	} else {
		verbose_print("\nUnmapped segment successfully. ");
	}
	//TODO: need to close file also
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
	GList* keys;
	int tid, i;

	if (trans_mapping == NULL) {
		tid = 1;
		trans_mapping = g_hash_table_new(g_int_hash, g_int_equal);
	} else {
		keys = g_hash_table_get_keys(trans_mapping);
		tid = g_list_length(keys) + 1;
	}

	trans_info_t* info = (trans_info_t*) malloc(sizeof(trans_info_t));
	info -> trans_seg_list = NULL;
	info -> old_value_items = NULL;
	for (i = 0; i < numsegs; i++) {
		seg_t* seg = find_seg(segbases[i]);
		if (seg == NULL) {
			fprintf(stderr,
					"\nError. rvm_begin_trans has a segbase that is not mapped");
			abort();
		}
		char temp_str[MSG_LEN];
		snprintf(temp_str, MSG_LEN,
				"\nrvm_begin_trans: Adding %s segment to transaction\n",
				seg -> name);
		verbose_print(temp_str);
		info -> trans_seg_list = g_list_append(info -> trans_seg_list, seg);

	}
	int* tid_cpy = (int*) malloc(sizeof(int));
	*tid_cpy = tid;
	printf("Inserted tid %d\n", *tid_cpy);
	g_hash_table_insert(trans_mapping, tid_cpy, info); // mapping b/w tid and trans_info


	return tid;
}

void rvm_about_to_modify(trans_t tid, void* segbase, int offset, int size) {
	seg_t* seg = find_seg(segbase);
	trans_item_t* item = (trans_item_t*) malloc(sizeof(trans_item_t));

	if (seg == NULL) {
		fprintf(stderr,
				"\nrvm_about_to_modify: segment not found for given tid. ");
		abort();
	}
	item -> seg = seg;
	item -> offset = offset;
	item -> size = size;
	item -> old_value = malloc(size);
	memcpy(item -> old_value, segbase + offset, size);

	trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
	//if (info -> old_value_items == NULL){
	//    info -> old_value_items = g_queue_new();
	//}
	//g_queue_push_tail(info -> old_value_items, item);
	info -> old_value_items = g_list_append(info -> old_value_items, item);
	//fprintf(stderr, "\naBOUTING: segname: %s, offset: %d, size: %d", seg-> name, item -> offset, item -> size);


	//trans_item_t* item1 = (trans_item_t*) g_queue_pop_tail(info -> old_value_items);
	//seg_t* seg1 = item1 -> seg;
	//fprintf(stderr, "\nREMOVING IMM: segname: %s, offset: %d, size: %d", seg1-> name, item1 -> offset, item1 -> size);
}

void rvm_commit_trans(trans_t tid) {
	trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
	GList* seg_list = info -> trans_seg_list;
	GList* item_list = info -> old_value_items;
	seg_t* seg;
	GList* iterator;
	FILE* log_file;
	log_file = fopen("rvm_segments/txn.log", "a");
	int size;
	char* value;
	trans_item_t* trans_item;
	/*
	 for(iterator = item_list; iterator; iterator = iterator -> next){
	 trans_item = (trans_item_t*)iterator -> data;
	 fprintf(stderr, "\noffset = %d, size =  %d \n", trans_item->offset,  trans_item->size);
	 value = (char*)((trans_item->seg)->mem + trans_item->offset);
	 fprintf(stderr, "\n seg name = %s, seg size =  %d and value = %s\n",(trans_item->seg)->name, (trans_item->seg)->size, value);
	 fprintf(log_file, "%s~%d~%d~%s", (trans_item->seg)->name, (trans_item->seg)->size, trans_item->offset, value);
	 fputs("\n", log_file);
	 }*/

	// for each segment one commit record

	for (iterator = seg_list; iterator; iterator = iterator -> next) {
		seg = (seg_t*) iterator -> data;

		fprintf(log_file, "%s~%d~", seg -> name, seg -> size);
		fwrite(seg -> mem, seg -> size, 1, log_file);
		fprintf(log_file, "\n");
		//fputs("\n", log_file);

		// the issue here is that while reading from the log, I need to read only the values and not the memory bits
		// one way is that we can store values and the offsets and not the memory chunk itself
		// if we are writing that in the log then it is better to write it to the disk segment itself
		// i guess we will have to use the offsets to get values and store only values in the log
	}

	fclose(log_file);
	verbose_print("\nTransaction committed");

}
void rvm_abort_trans(trans_t tid) {
	trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
	//GQueue* old_values = info -> old_value_items;


	seg_t* segu = (seg_t*) g_list_first(mapped_seg_list) -> data;
	GList* it = g_list_last(info -> old_value_items);
	for (; it != NULL; it = g_list_previous(it)) {
		trans_item_t* item = (trans_item_t*) (it -> data);
		seg_t* seg = item -> seg;

		fprintf(stderr, "\naborting: segname: %s, offset: %d, size: %d",
				seg-> name, item -> offset, item -> size);
		fprintf(stderr, "\naborting: current contents\n");
		fwrite(seg -> mem, seg->size, 1, stderr);

		fprintf(stderr, "\naborting: contents which overwrite\n");
		fwrite(item -> old_value, item -> size, 1, stderr);

		memcpy((seg -> mem) + item -> offset, item -> old_value, item -> size);

	}

	//while((info -> old_value_items) != NULL && g_queue_get_length(info -> old_value_items)){
	//    //read list of old value items backwards and apply them
	//    //on the segments in memory for all items for this tid.
	//    trans_item_t* item = (trans_item_t*) g_queue_pop_tail(info -> old_value_items);


	//}

	verbose_print("\nrvm_abort_trans: Successfull abort");
}

void rvm_truncate_log(rvm_t rvm) {
	const char null_char[10] = "";
	writeDataToDisk(rvm, null_char, 1);
}

void rvm_destroy(rvm_t rvm, const char *segname) {
	char cmd[FILE_NAME_LEN + 10];
	snprintf(cmd, 50, "rm ./%s/%s.seg", rvm.dir_path, segname);
	system(cmd);
}

/* int main(){
 char* buffers[1];
 int i;
 GList* iterator = NULL;
 rvm_t rvm = rvm_init("store");
 fprintf(stderr, "\ncreated path: %s", rvm.dir_path);
 buffers[0] = (char*)rvm_map(rvm, "seg0", 10);
 //buffers[1] = (char*)rvm_map(rvm, "seg1", 10);
 //buffers[2] = (char*)rvm_map(rvm, "seg2", 10);

 snprintf(buffers[0]+1, 10, "BLAH1");
 //snprintf(buffers[1]+2, 10, "BLAH2");
 //snprintf(buffers[2]+3, 10, "BLAH3");
 seg_t* segu = (seg_t*)g_list_first(mapped_seg_list) -> data;
 fprintf(stderr, "\nPoint1:\n");
 fwrite(segu -> mem, segu -> size, 1, stderr);
 fprintf(stderr, "\nBEFORE\n\n");
 fprintf(stderr, "\nbuffer[0]:\n");
 fwrite(buffers[0], 10, 1, stderr);
 fflush(stderr);
 //fprintf(stderr, "\nbuffer[1]:\n");
 //fwrite(buffers[1], 10, 1, stderr);
 //fflush(stderr);

 //fprintf(stderr, "\nbuffer[2]:\n");
 //fwrite(buffers[2], 10, 1, stderr);
 //fflush(stderr);


 trans_t tid = rvm_begin_trans(rvm, 1, (void**)buffers);
 fprintf(stderr, "\ntid: %d ", tid);


 rvm_about_to_modify(tid, buffers[0], 1, 6);
 //snprintf(buffers[0]+1, 10, "TEST1");
 //rvm_about_to_modify(tid, buffers[1], 2, 6);
 //snprintf(buffers[1]+2, 10, "TEST2");
 //
 //rvm_about_to_modify(tid, buffers[2], 3, 6);
 //snprintf(buffers[2]+3, 10, "TEST3");


 //rvm_commit_trans(tid);
 //rvm_abort_trans(tid);
 fprintf(stderr, "\nAFTER\n\n");
 fprintf(stderr, "\nbuffer[0]:\n");
 fwrite(buffers[0], 10, 1, stderr);
 fflush(stderr);
 //fprintf(stderr, "\nbuffer[1]:\n");
 //fwrite(buffers[1], 10, 1, stderr);
 //fflush(stderr);

 //fprintf(stderr, "\nbuffer[2]:\n");
 //fwrite(buffers[2], 10, 1, stderr);
 //fflush(stderr);
 rvm_unmap(rvm, (void*)buffers[0]);
 //rvm_unmap(rvm, (void*)buffers[1]);
 //rvm_unmap(rvm, (void*)buffers[2]);

 fprintf(stderr, "\n\n");
 abort();
 }*/
