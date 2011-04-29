#include <glib.h>
#define LOG_NAME "txn.log" //log used for all transaction of all segments
#define FILE_NAME_LEN 50
#define MSG_LEN 1000


typedef struct rvm{
    const char* dir_path;
    const char* log_path; //one log for all segments in directory
}rvm_t;

typedef struct seg{
    const char* name;
    int size;
    void* mem;
}seg_t;

typedef struct trans_item{
    seg_t* seg;
    int size;
    int offset;
    void* old_value; //array of old values
}trans_item_t;

typedef struct trans_info{
    GList* trans_seg_list; //list of segments used in transaction
    GList* old_value_items; //list of trans_items stored with rvm_about_to_modify
}trans_info_t;

typedef int trans_t;

rvm_t rvm_init(const char* directory);

void* rvm_map(rvm_t rvm, const char* segname, int size_to_create);

void rvm_unmap(rvm_t rvm, void* segbase);

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);

void rvm_about_to_modify(trans_t tid, void* segbase, int offset, int size);

void rvm_commit_trans(trans_t tid);

void rvm_abort_trans(trans_t tid);
void rvm_truncate_log(rvm_t rvm);
