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
	close(f_desc);
	return status;
}


seg_t* find_seg(void* segbase) {
	GList* iterator = mapped_seg_list;
	for (; iterator; iterator = iterator->next) {
		seg_t* seg = (seg_t*) (iterator -> data);
		//fprintf(stderr, "\n In find_seg - size of the segment  = %d", seg->size);
		if (seg -> mem == segbase) {
			//fprintf(stderr,"\n In find_seg - !! size of the segment selected   = %d", seg->size);
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
		if (!file_exists(log_path)) {
			fprintf(stderr, "\nrvm_init: Existing directory does not have log file.");
			abort();
		}
	}
	rvm.dir_path = directory;
	mapped_seg_list = NULL;
	rvm.log_path = log_path;
	fprintf(stderr, "\nrvm_init: Log path of rvm is set to - %s\n", rvm.log_path);
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
		fprintf(stderr, "\nwriteDataToDisk: Segname in log: %s", curr_segname);
		if (!trunc_flag && strcmp(segname, curr_segname))
			continue;
		char curr_segname_full[FILE_NAME_LEN];
		snprintf(curr_segname_full, FILE_NAME_LEN, "%s/%s.seg", rvm.dir_path, curr_segname);
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
		curr_segsize[j] = '\0';
		seg_size = atoi(curr_segsize);

		j = 0;
		for (++i; j < seg_size; j++, i++) {
			fputc(buffer[i], seg_file);
		}
		char verbose_buffer[100];
		snprintf(verbose_buffer, 100, "\nRecovered/Flushed %s commit record to backing store", curr_segname); 
		verbose_print(verbose_buffer);
		fclose(seg_file);
		//fprintf(stderr, "\n In writedatatodisk: file_size of the segment %s = %d ", curr_segname_full, file_size(curr_segname_full));
	}
	
	fclose(log_file);
	if (trunc_flag){
		FILE* log_to_del = fopen(rvm.log_path, "w");
		if (fclose(log_to_del)){
			perror("\nLog file truncation failed");
		}
		else{
			verbose_print("\nLog file truncated successfully");
		}
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
		fd = open(file_path, O_RDWR, (mode_t) 0600);
		//fprintf(stderr, "\n In rvm_map - Checking file size, do we need to expand, file_len = %lu, size_to_create = %d\n", file_len, size_to_create);
		if (file_len < size_to_create) {
			//Seek to one byte before required size and write
			//one byte so that file is extended

			if (!fd) {
				perror("Error opening existing segment file");
				exit(EXIT_FAILURE);
			}
			result = lseek(fd, 0, SEEK_SET);
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
			char verbose_buffer[100];
			snprintf(verbose_buffer, 100, "\n%srvm_map: Segment expanded to size: %ld", segname, file_len); 
			verbose_print(verbose_buffer);

			//close(fd);
		}

		fprintf(stderr, "\nrvm_map: Read contents from segment at %s of size %d",
				file_path, size_to_create);

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
		fprintf(stderr, "\nrvm_map: Created segment %s in %s", segname, file_path);
		//close(fd);

		//create an empty buffer
		//buffer = calloc(1, size_to_create);
	}

	buffer = mmap(0, size_to_create, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
					0);
	if (buffer == MAP_FAILED) {
		perror("\nrvm_map mmap failed ");
	}

	seg_t* segment = (seg_t*) malloc(sizeof(struct seg));
	segment->name = segname;
	segment->size = size_to_create;
	segment->mem = buffer;

	mapped_seg_list = g_list_append(mapped_seg_list, segment);
	char verbose_buffer[100];
	snprintf(verbose_buffer, 100, "\nrvm_map successful for segment: %s \n", segname); 
	verbose_print(verbose_buffer);

	return buffer;

}

void rvm_unmap(rvm_t rvm, void* segbase) {
	seg_t* seg = find_seg(segbase);
	if (seg == NULL) {
		fprintf(stderr, "\nrvm_unmap: Segment not found. Cannot be unmapped.");
		abort();
	}

	if (munmap(segbase, seg -> size) == -1) {
		perror("Error un-mmapping the file");
	} else {
	
		GList* find_seg = g_list_find(mapped_seg_list, seg);
		//fprintf(stderr, "\n In unmap mapped_seg_list size is = %d\n", g_list_length(mapped_seg_list));
		mapped_seg_list = g_list_remove_link(mapped_seg_list, find_seg);
		//fprintf(stderr, "\n In unmap mapped_seg_list size should decrease to = %d\n", g_list_length(mapped_seg_list));
		
		verbose_print("\nrvm_unmap: Unmapped segment successfully. ");
	}
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
					"\nrvm_begin_trans: Error. rvm_begin_trans has a segbase that is not mapped");
			abort();
		}
		char temp_str[MSG_LEN];
		snprintf(temp_str, MSG_LEN,
				"\nrvm_begin_trans: Adding %s segment to transaction and the trans_seg_list size is = %d\n",
				seg -> name, g_list_length(info->trans_seg_list));
		verbose_print(temp_str);
		info -> trans_seg_list = g_list_append(info -> trans_seg_list, seg);

	}

	rvm_t* rvm1 = (rvm_t*)malloc(sizeof(rvm_t));
	char* dir_path = (char*)malloc(FILE_NAME_LEN * sizeof(char));
	strcpy(dir_path, rvm.dir_path);
	rvm1 -> dir_path = dir_path;

	char* log_path1 = (char*)malloc(FILE_NAME_LEN * sizeof(char));
	strcpy(log_path1, rvm.log_path);
	rvm1 -> log_path = log_path1;
	info -> rvm = rvm1;
	int* tid_cpy = (int*) malloc(sizeof(int));
	*tid_cpy = tid;
	g_hash_table_insert(trans_mapping, tid_cpy, info); // mapping b/w tid and trans_info

	char temp_str[MSG_LEN];
	snprintf(temp_str, MSG_LEN, "\nrvm_begin_trans: tid assigned = %d\n", tid);
	verbose_print(temp_str);

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
	info -> old_value_items = g_list_append(info -> old_value_items, item);
	
	
	char verbose_buffer[100];
	snprintf(verbose_buffer, 100, "\nrvm_about_to_modify: old values saved for segment: %s, offset: %d, size: %d", 
		seg -> name, offset, size); 
	verbose_print(verbose_buffer);
}

void rvm_commit_trans(trans_t tid) {
	trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
	GList* seg_list = info -> trans_seg_list;
	GList* item_list = info -> old_value_items;
	seg_t* seg;
	GList* iterator;
	FILE* log_file;
	int size;
	char* value;
	trans_item_t* trans_item;
	char dir_path[FILE_NAME_LEN];
	
	//fprintf(stderr, "\n In rvm_commit , transaction id =%d\n", tid);
	//fprintf(stderr,"\n In rvm_commit, seg list length= %d and item list - %d",	g_list_length(seg_list), g_list_length(item_list));
	snprintf(dir_path, FILE_NAME_LEN, "%s/%s", info -> rvm -> dir_path, LOG_NAME);
	log_file = fopen(dir_path, "a");
	// for each segment one commit record
	for (iterator = seg_list; iterator; iterator = iterator -> next) {
		seg = (seg_t*) iterator -> data;
		
		fprintf(log_file, "%s~%d~", seg -> name, seg -> size);
		fwrite(seg -> mem, seg -> size, 1, log_file);
		fprintf(log_file, "\n");
		//fprintf(stderr, "\n In rvm_commit_trans: Inside loop: Committing transaction  with seg name = %s and seg size = %d\n", seg->name, seg->size);
	}

	fclose(log_file);
	verbose_print("\nrvm_commit_trans: Transaction committed");

}
void rvm_abort_trans(trans_t tid) {
	trans_info_t* info = g_hash_table_lookup(trans_mapping, &tid);
	seg_t* segu = (seg_t*) g_list_first(mapped_seg_list) -> data;
	GList* it = g_list_last(info -> old_value_items);
	for (; it != NULL; it = g_list_previous(it)) {
		trans_item_t* item = (trans_item_t*) (it -> data);
		seg_t* seg = item -> seg;

		fwrite(seg -> mem, seg->size, 1, stderr);

		fwrite(item -> old_value, item -> size, 1, stderr);

		memcpy((seg -> mem) + item -> offset, item -> old_value, item -> size);

	}
	verbose_print("\nrvm_abort_trans: Transaction aborted successfully.");
}

void rvm_truncate_log(rvm_t rvm) {
	const char null_char[10] = "";
	writeDataToDisk(rvm, null_char, 1);
	verbose_print("\nrvm_truncate_log: Truncation successful");
}

void rvm_destroy(rvm_t rvm, const char *segname) {
	char cmd[FILE_NAME_LEN + 10];
	snprintf(cmd, 50, "rm ./%s/%s.seg", rvm.dir_path, segname);
	system(cmd);
	char verbose_buffer[100];
	snprintf(verbose_buffer, 100, "\nrvm_destroy: Deleting backing store for segment: %s", segname); 
	verbose_print(verbose_buffer);
}

