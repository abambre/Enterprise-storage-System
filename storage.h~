#ifndef STORAGE_H
#define STORAGE_H

#include<stdio.h>
#include<stdlib.h>

typedef enum {Nfile, Ndir} Ntype;

struct block_t {   
  long blk_num;
  struct block_t *nxt_blk;
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
};

typedef struct node_t *Node;


/*Singly linked list of all files - For cold file track*/
typedef struct List_item {
	Node inode; 	
	struct List_item *next;
}List_item ;

List_item *acclist_head = NULL;
//List_item *fs_head = NULL;

#endif
