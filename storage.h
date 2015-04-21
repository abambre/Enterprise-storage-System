#ifndef STORAGE_H
#define STORAGE_H

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <dropbox.h>
#include <memStream.h>
#include <glib.h>
#include "fuse_common.h"
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#define FILENAME_SIZE 30
#define FULLPATHNAME 1000
#define BLOCK_SIZE 4096
#define MB_CONVERT 1024*1024
#define MAX_STORAGE_THRESHOLD 70
#define OPTIMAL_STORAGE_THRESHOLD 50
#define MIN_STORAGE_THRESHOLD 30
#define KRED  "\x1B[31m"
#define RESET "\033[0m" 

#define DEBUG
#ifdef DEBUG
#  define D(x) printTime(); x
#else
#  define D(x) 
#endif

#define TRACE
#ifdef TRACE
#  define T(x) printTime(); x
#else
#  define T(x) 
#endif

typedef enum {Nfile, Ndir} Ntype;

typedef enum {False, True } Bool;

struct block_t {
	long blk_num;
	struct block_t *nxt_blk;
	char * server_block_hash;
	Bool inmemory_flag;
};

typedef struct block_t *Block;

// Main inode structure
struct node_t {
	Ntype type;
	char name[FILENAME_SIZE];
	struct node_t *child;
	struct node_t *next;
	Block data;
	int len;
	time_t access_time;
	Bool inmemory_node_flag;
	pthread_mutex_t lock;
};

typedef struct node_t *Node;


/*Singly linked list of all files - For cold file track*/
typedef struct List_item {
	Node inode;
	struct List_item *next;
}List_item ;

drbClient * intialize_drop_box();
void move_to_dropbox_cold_storage(char * );
void fetch_from_dropbox_cold_storage(char * );
void upload_dropbox_file(char *, char *, char *);
int download_dropbox_file(char *, char *, char *);
int connect_to_dropbox(drbClient* , char *, char *);
void write_access_cold_blocks(Node);
void print_cold_blocks(Block);
char * hash_calc(Block);
void iterator(gpointer, gpointer, gpointer);
void create_hashtree();
void activate_hashtree();
gboolean hashtree_contains(char *);
void write_block_to_file(Block, char * );
void update_hashtree(char *);
void read_block_from_file(char *, Block);
void read_access_cold_blocks(Node);

int checkStorageThreshold(int);
void insert_back(Node, List_item **);
Node get_inode(List_item **);
List_item *sort_list(List_item *);
List_item *sort_list_size(List_item *);
void print_list(List_item *);
int populate_access_list();
long calc_blcks_transfer(int);
int calc_file_blks(int);
void prepare_nodelist_to_transfer();
void *track_cold_files();
int checkMinStorageThreshold(int);
int populate_retrieval_list();
void print_retrieval_list();
List_item *sort_rlist(List_item *);
void *get_cold_files();

// List for access and retrival of files.
extern List_item *acclist_head;
extern List_item *transfer_list;
extern List_item *rtvlist_head;

// Flags
extern int thread_flag;

// Benchmark Time
extern time_t latest_file_access_time;

// Locks
extern pthread_mutex_t count_lock;

// Array maintaining free blocks
extern int *free_blk;

// defining root node here
extern Node root,buffNode;
extern char **memory_blocks;
// As there is input limit for the memory defining global variables

// Memory Counter variables
extern long long malloc_counter,malloc_limit,block_count, free_block_count;
extern int block_number;

// Dropboc Client and HashTree Data Structure
extern drbClient *cli;
extern GHashTable* hashtree;

//Timer variables
extern time_t timer;
extern char buffer[26];
extern struct tm* tm_info;


#endif
