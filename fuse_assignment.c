
/*
Author : Aditya Ambre
Fuse based File system which supports POSIX functionalities.

  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall rmfs.c `pkg-config fuse --cflags --libs` -o rmfs
*/

#define FUSE_USE_VERSION 26
#define FILENAME_SIZE 30
#define FULLPATHNAME 1000 
#define BLOCK_SIZE 4096
#define MB_CONVERT 1024*1024

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include "fuse_common.h"
#include <time.h>
#include <pthread.h>

/*
Initially all the function were to make sure reusability is maintain, but method switching cost is verfied after
running postmark program. Hence lookup code is repeated in the all the function to make it faster.
*/


typedef enum {Nfile, Ndir} Ntype;


struct block_t {   
  long blk_num;
  struct block_t *nxt_blk;
};

typedef struct block_t *Block;

// Array maintaining free blocks
int *free_blk=NULL;

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

// defining root node here 
Node root,buffNode;
char **memory_blocks=NULL;
// As there is input limit for the memory defining global variables 

long long malloc_counter=0,malloc_limit=0,block_count=0, free_block_count=0;

int checkStorageThreshold()
{
  return (int)((free_block_count/block_count)*100) > 60 ? 1:0;
}

int getFreeBlock()
{
  int i;

  for(i=0;i<block_count;i++)
  {
    if(free_blk[i]==-1)
    {
      free_blk[i]=0;
      free_block_count++;
      return i;
    }  
  }

  return -1;
}
/*Singly linked list of all files - For cold file track*/
typedef struct List_item {
	Node inode; 	
	struct List_item *next;
}List_item ;

//List_item *head = NULL;

void insert_back(Node inode, List_item **head) {
	if ((head == NULL) || (inode == NULL)) return;
	List_item *new_node = NULL;
	List_item *curr = NULL;

	new_node = (List_item *)malloc(sizeof(List_item));
  if(NULL == new_node) {
    printf("Malloc Error\n");
    return;
 	}
	new_node->inode = inode;
	new_node->next = NULL;

	//List empty
	if (NULL == *head) {
		*head = new_node;
		return;
	}	
	
	//Insert back
	curr = *head;
	while(curr->next != NULL) {
		curr = curr->next;
	}
	curr->next = new_node;
	return;
}

/* Remove from front of List - Least accessed File/Cold File */
Node get_inode(List_item **head) {
	if(head == NULL) return NULL;
	Node ret;
	List_item *temp = *head;
	if(*head == NULL) return NULL;
	*head = (*head)->next;
	ret = temp->inode;
	free(temp);
	return ret;
}

/*Sort the list according to access_time*/
List_item *sort_list(List_item *lroot) {
	double seconds;
	List_item *curr = NULL, *least = NULL, *least_prev = NULL, *prev = NULL;
	List_item *tmp = NULL;
	
	if ( (lroot == NULL) || (lroot->next == NULL) ) 
		return lroot;
		
	curr = least = least_prev = prev = lroot;
	while(curr != NULL) {
		seconds = difftime(least->inode->access_time,curr->inode->access_time);
  	if (seconds > 0) {  // printf("Date1 > Date2\n");
  		                  //if (curr->inode->access_time < least->inode->access_time) {
			least_prev = prev;
			least = curr;
		}
		prev = curr;
		curr = curr->next;		
	}	
	
	if(least != lroot) {
		least_prev->next = lroot;
		tmp = lroot->next;
		lroot->next = least->next;
		least->next = tmp;	
	}

	least->next = sort_list(least->next);
	return least;
}

void print_list(List_item **acclist_head) {
	List_item *temp = NULL;
	if (acclist_head == NULL) {
		printf("List head empty\n");
		return;
	}
	if (*acclist_head == NULL) {
		printf("List is empty\n");
		return;
	}
	temp = *acclist_head;
	printf("Printing File access list\n");
	while(temp != NULL) {
		printf("%s:%ld->",temp->inode->name, temp->inode->access_time);
		temp = temp->next;
	}
}

void populate_list (List_item **acclist_head) {
	Node temp = root;
	List_item *fs_head = NULL;
	
	if ((root->child == NULL) && (root->next == NULL)) return;
	insert_back(root, &fs_head);
	
	while(fs_head != NULL) {
		temp = get_inode(&fs_head);
		temp = temp->child;
		while (temp != NULL) {
			if(temp->type == Ndir) {
				insert_back(temp, &fs_head);
			}
			else if(temp->type == Nfile) {
				insert_back(temp, acclist_head);
			}
			temp = temp->next;
		}
	}	
}

/*-----------Track cold Files---------*/

void *track_cold_files() {
	printf("One Thread getting called\n");
	sleep(120);
	List_item *acclist_head = NULL;
	printf("Thread getting called\n");
	
	populate_list(&acclist_head);
	print_list(&acclist_head);	
	printf("\nDone printing\n");
	acclist_head = sort_list(acclist_head);
	printf("\nSorting done\n");
	print_list(&acclist_head);	
	printf("\nSecond Printing  done\n");
	pthread_exit(NULL);
}

// This function creates heap memory for the Node with specified size
int ckmalloc(unsigned l,Node *t)
{
  void *p;
  /*
  if(malloc_limit < (malloc_counter+l))
  {
    return ENOSPC;
  }  
  */
  p = malloc(l);
  if ( p == NULL ) {
    perror("malloc");
    return errno;
  }
  memset(p,'\0',l);
  *t=p;
  //malloc_counter+=l;

  return 0;
}

// This function creates heap memory for the array with specified size
 int ckmalloc_w(unsigned l,char **t)
{
  void *p;

  if(malloc_limit < (malloc_counter+l))
  {
    return ENOSPC;
  }

  p = malloc(l);
  if ( p == NULL ) {
    perror("malloc");
    return errno;
  }
  memset(p,'\0',l);
  *t=p;
  //malloc_counter+=l;
  return 0;
}

void freeBlock(Block blk)
{
  if(blk==NULL)
    return;
  freeBlock(blk->nxt_blk);
  free_blk[blk->blk_num]=-1;
  memset(memory_blocks[blk->blk_num],'\0',BLOCK_SIZE);
  free(blk);
  free_block_count--;
}

// freess memory
void freemalloc(Node n)
{
  long totalsize=0;
  if(n->data!=NULL)
  {
    totalsize+=n->len;
    freeBlock(n->data);
  }
  totalsize+=sizeof(*root);
  free(n);
  //malloc_counter-=totalsize;
}

//  Lookup function to serach for the particular node by traversing the path from root
static int directory_lookup(const char *path,Node *t,int mode)
{
	char lpath[100];
	memset(lpath,'\0',100);
	memcpy(lpath,path,strlen(path));	

	char *token=strtok(lpath, "/");
	Node temp=root,prev=NULL;

    while( token != NULL) 
    {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
     // printf( " <%s>\n", token );

      while(temp!=NULL) {
      	if (strcmp(token, temp->name) == 0) {
      		printf("found %s \n",temp->name );
      		break;
        }      
      	temp=temp->next; 
  	  }
      if(temp==NULL)
      {
      	if(mode==1) *t=prev;
      	return -1;
      }

      token = strtok(NULL, "/");
    }    

    *t=temp;
    return 0;
}

/*
static int delete_node(const char *path,Ntype t)
{
//	printf("inside the delete_node %s\n",path);
	char lpath[100];
	memset(lpath,'\0',100);
	memcpy(lpath,path,strlen(path));	

	char *token=strtok(lpath, "/");
	Node temp=root,prev=NULL,sib;

	while( token != NULL) 
    {
	  prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
      	if (strcmp(token, temp->name) == 0) {
      		printf("found %s \n",temp->name );
      		break;
        }
        sib=temp;      
      	temp=temp->next; 
  	  }
      if(temp==NULL)
      {
      	return -1;
      }
      token = strtok(NULL, "/");
    }

    if(temp->type==Ndir && temp->child != NULL)
    {
       return ENOTEMPTY;
    }

    if(sib==NULL)
    {
    	prev->child=temp->next;
    }
    else if(sib!=NULL)
    {
    	sib->next=temp->next;
    }


    freemalloc(temp);
    return 0;
}
*/

// This function used for getting attribute information for directory or file
static int rmfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
  Node temp=root;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
  else {  
    char lpath[100];
    memset(lpath,'\0',100);
    memcpy(lpath,path,strlen(path));  

    char *token=strtok(lpath, "/");

    while( token != NULL) 
    {
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }

      token = strtok(NULL, "/");
    }    
    
    if(temp!=NULL)
    {
		  stbuf->st_mode = S_IFREG | 0777;
		  stbuf->st_size = 0;
		  if(temp->type==Ndir)
		  	stbuf->st_mode = S_IFDIR | 0777;
		  else if(temp->data !=NULL)
			 stbuf->st_size=temp->len;
		  stbuf->st_nlink = 1;
		}
	 else 
		  res = -ENOENT;
  }
  buffNode=temp;
  //printf("<<<<<<< %lld  , %lld>>>>>>>\n",malloc_counter,malloc_limit );
	return res;
}

// For readinf directory contents
static int rmfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

  dirNode=dirNode->child;
  while(dirNode!=NULL)
  {
    //printf("node dir %s\n",dirNode->name);
		filler(buf, dirNode->name, NULL, 0);
		dirNode=dirNode->next;
  }

	return 0;
}
	
 // before opening a file it calls to check whether file exist hence lookup 
static int rmfs_open(const char *path, struct fuse_file_info *fi)
{
	char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

	return 0;
}

// Read the actual content of file by first traversing and populating buf
static int rmfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	 char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

  if(dirNode->type==Ndir)
    	return -EISDIR;

  if(dirNode->data==NULL)
    	return 0;

	len = dirNode->len;
	dirNode->access_time = time(NULL);
	
	if (offset < len) {  
		if (offset + size > len)
			size = len - offset;

    Block offset_blk=dirNode->data;
    //Block prev_blk=dirNode->data;
    int offset_loop=1;
    while(offset > (BLOCK_SIZE * offset_loop))
    {
     // prev_blk=offset_blk;
      offset_blk=offset_blk->nxt_blk;
      offset_loop++;
    }
  	//	memcpy(buf, rmfs_str + offset, size);
    int buff_size=size,tmp_offset=offset;
    int charcpy=0;

    while(charcpy < size)
    {  

      int dmove= (tmp_offset+buff_size)>(BLOCK_SIZE*offset_loop)?(BLOCK_SIZE*offset_loop - tmp_offset): buff_size;
      int recal_offset= tmp_offset - (BLOCK_SIZE*(offset_loop - 1));
      memcpy((buf+ charcpy),memory_blocks[offset_blk->blk_num] + recal_offset,dmove);
      charcpy+=dmove;
      buff_size-=dmove;
      tmp_offset=BLOCK_SIZE*offset_loop;
      offset_loop++;
     // prev_blk=offset_blk;
      offset_blk=offset_blk->nxt_blk;
	  }	

	} else
		size = 0;

	//printf("in the read buf : <%s> and size <%d>\n", buf,size);
	return size;
}


// get file name from the path string passed
static void getFileName(const char *path,char lastname[])
{
	char spiltstr[100];
	memset(spiltstr,'\0',100);
	memcpy(spiltstr,path,strlen(path));
	char *token=strtok(spiltstr, "/");
	char *lastslsh;
    while( token != NULL ) 
    {
    	lastslsh=token;
    	token = strtok(NULL, "/");
    }	
	strncpy(lastname,lastslsh,strlen(lastslsh));
    return;
}

/*
static void getLastChild(Node *t)
{
	Node child=*t;
	child=child->child;
	while(child->next!=NULL)
    {
    //	printf("inside the getlastchildloop %s\n",child->name);
		child=child->next;
    }
    *t=child;
}
*/

// Make directory 
static int rmfs_mkdir(const char *path, mode_t mode)
{
	Node dirNode=NULL;
	Node newDirNode=NULL;
	char a[FILENAME_SIZE];
	memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

  /*
  if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }
  */

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
  //malloc_counter+=sizeof(*root);

  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Ndir;
  //newDirNode->data->blk_num=-1;
  newDirNode->access_time = time(NULL);

  newDirNode->data=NULL;
  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  {	
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
  	dirNode->next=newDirNode;
  	//printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
	return 0;
}

// Optional  but can be useful if making node 
static int rmfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	Node dirNode=NULL;
  Node newDirNode=NULL;
  char a[FILENAME_SIZE];
  memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

  if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
//malloc_counter+=sizeof(*root);
  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Nfile;
  //newDirNode->data->blk_num=-1;
  newDirNode->data=NULL;
  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  { 
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
    dirNode->next=newDirNode;
    //printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
  return 0;
}


static int rmfs_flush(const char *path, struct fuse_file_info *fi)
{ 	/*
	printf("printing all root childs\n");
	Node tcp=root;
	tcp=tcp->child;

	while(tcp !=NULL)
	{
		printf("node -> %s\n",tcp->name );
		tcp=tcp->next;
	}
	*/
	return 0;
}

//create file under specific path
static int rmfs_create(const char *path, mode_t t,struct fuse_file_info *fi)
{ 	
  Node dirNode=NULL;
  Node newDirNode=NULL;
  char a[FILENAME_SIZE];
  memset(a,'\0',FILENAME_SIZE);
  char spiltstr[100];
  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  char *token=strtok(spiltstr, "/");
  char *lastslsh;
  while( token != NULL ) 
  {
    lastslsh=token;
    token = strtok(NULL, "/");
  } 
  strncpy(a,lastslsh,strlen(lastslsh));

  /*
  if(malloc_limit < (malloc_counter+sizeof(*root)))
  {
    return -ENOSPC;
  }
  */

  newDirNode = malloc(sizeof(*root));
  if ( newDirNode == NULL ) {
    perror("malloc");
    return errno;
  }
  //malloc_counter+=sizeof(*root);
  memset(newDirNode,'\0',sizeof(*root));

  newDirNode->type=Nfile;
  //newDirNode->data->blk_num=-1;
  newDirNode->data=NULL;
  newDirNode->access_time = time(NULL);

  strncpy(newDirNode->name,a,sizeof(a));

  memset(spiltstr,'\0',100);
  memcpy(spiltstr,path,strlen(path));
  token=strtok(spiltstr, "/");

  Node temp=root,prev=NULL;

  while( token != NULL) 
  {
      //printf("inside the loop\n");
      prev=temp;
      temp=temp->child;
      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          //printf("found %s \n",temp->name );
          break;
        }      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        break;
      }
      token = strtok(NULL, "/");
  }    

  dirNode=prev;

  if(prev->child !=NULL)
  { 
    prev=prev->child;
    while(prev->next!=NULL)
    {
      //  printf("inside the getlastchildloop %s\n",child->name);
      prev=prev->next;
    }
    dirNode=prev;
    dirNode->next=newDirNode;
    //printf("name is : %s\n",dirNode->next->name );
  }
  else
  {
    dirNode->child=newDirNode;
   // printf("name c is : %s\n",dirNode->child->name );
  }
  return 0;
}

// Remove the node 
static int rmfs_unlink(const char *path)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node temp=root,prev=NULL,sib;

  while( token != NULL) 
  {
      prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
          printf("found %s \n",temp->name );
          break;
        }
        sib=temp;      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        return -ENOENT;
      }
      token = strtok(NULL, "/");
  }

  if(sib==NULL)
  {
    prev->child=temp->next;
  }
  else if(sib!=NULL)
  {
    sib->next=temp->next;
  }

  freemalloc(temp);
	return 0;
}

// Remove directory
static int rmfs_rmdir(const char *path)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node temp=root,prev=NULL,sib;

  while( token != NULL) 
  {
      prev=temp;sib=NULL;
      temp=temp->child;

      while(temp!=NULL) {
        if (strcmp(token, temp->name) == 0) {
         // printf("found %s \n",temp->name );
          break;
        }
        sib=temp;      
        temp=temp->next; 
      }
      if(temp==NULL)
      {
        return -ENOENT;
      }
      token = strtok(NULL, "/");
  }

  if(temp->type==Ndir && temp->child != NULL)
  {
      return -ENOTEMPTY;
  }

  if(sib==NULL)
  {
    prev->child=temp->next;
  }
  else if(sib!=NULL)
  {
    sib->next=temp->next;
  }

  freemalloc(temp);
  return 0;
}

// Checking file/ dir exists
static int rmfs_access(const char *path, int mask)
{
	Node dirNode=NULL;
    int errChk=directory_lookup(path,&dirNode,0);
    if(errChk==-1)
    	return -ENOENT;
	return 0;
}


//truncate file
static int rmfs_truncate(const char *path, off_t size)
{
  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");
  Node dirNode=root;

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  if(dirNode==NULL)
    return -ENOENT;

  if(dirNode->type==Ndir)
    	return -EISDIR;

  if(dirNode->data==NULL)
    	return 0;

//malloc_counter-=dirNode->len;
    freeBlock(dirNode->data);
    dirNode->access_time = time(NULL);
	dirNode->data=NULL;
    //memset(dirNode->data,'\0',strlen(dirNode->data));
	return 0;
}

static int rmfs_ftruncate(const char *path, off_t size,struct fuse_file_info *fi)
{
	return 0;
}

static int rmfs_chmod(const char *path, mode_t t)
{
	return 0;
}

static int rmfs_chown(const char *path,uid_t uid, gid_t gid)
{
	return 0;
}

static int rmfs_utimens(const char* path, const struct timespec ts[2])
{
	return 0;
}


// write content to the file
static int rmfs_write(const char *path, const char *buf, size_t size,
off_t offset, struct fuse_file_info *fi)
{
	
  Node dirNode=root;

  char lpath[100];
  memset(lpath,'\0',100);
  memcpy(lpath,path,strlen(path));  

  char *token=strtok(lpath, "/");

  while( token != NULL) 
  {
    dirNode=dirNode->child;
    while(dirNode!=NULL) {
      if (strcmp(token, dirNode->name) == 0) {
          //printf("found %s \n",dirNode->name );
          break;
      }      
        dirNode=dirNode->next; 
    }
    if(dirNode==NULL)
    {
      break;
    }
    token = strtok(NULL, "/");
  }

  //dirNode=buffNode;

  if(dirNode==NULL)
    return -ENOENT;
  else if(dirNode->type==Ndir)
    	return -EISDIR;
  else if(buf==NULL)
    	return 0;

  int buff_size=size,tmp_offset=offset;
  Block prev_blk=NULL;

  // printf(" Before the actual writing \n" );
  int fblk,charcpy=0;
  if(dirNode->data==NULL)
  {
    //printf(" Inside the data -> NULL and data is <%s>\n",buf );
    dirNode->data=prev_blk=malloc(sizeof(struct block_t));
    if(dirNode->data == NULL)
    {
      perror("malloc");
      return errno;
    }

    fblk=getFreeBlock();
    dirNode->data->blk_num=fblk;
    dirNode->data->nxt_blk=NULL;
   // printf("Wriitn in the block <%d>\n",fblk );

    while(charcpy < size)
    {  
      int dchar=(buff_size > BLOCK_SIZE?BLOCK_SIZE:buff_size);
      //printf("Before writing in the block tmp_offset <%d> dchar <%d>\n",tmp_offset,dchar );

      strncpy((memory_blocks[fblk] + tmp_offset),(buf+ charcpy),dchar);
      charcpy+=dchar;
      buff_size-=BLOCK_SIZE;
      tmp_offset=0;
      
      if(charcpy < size)
      {
        //printf("Shouldn't be here\n");
        fblk=getFreeBlock();
        prev_blk->nxt_blk=malloc(sizeof(struct block_t));
        prev_blk->nxt_blk->blk_num=fblk;
        prev_blk->nxt_blk->nxt_blk=NULL;
        if(prev_blk->nxt_blk == NULL)
        {
          perror("malloc");
          return errno;
        }
        prev_blk=prev_blk->nxt_blk;
      } 
      
    }
    //printf("Aftet the exist data <%s>\n",memory_blocks[dirNode->data->blk_num]);
  } 
  else
  {

    //printf("Inside the appended section offset <%d> size <%d> block number <%d>\n",offset,size ,dirNode->data->blk_num);
    Block offset_blk=dirNode->data;
    int offset_loop=1;
    while(offset > (BLOCK_SIZE * offset_loop))
    {
      prev_blk=offset_blk;
      offset_blk=offset_blk->nxt_blk;
      //printf("Next block is : <%d>\n", offset_blk->blk_num);
      offset_loop++;
    }

    while(charcpy < size)
    {  
      if(offset_blk==NULL)
      {
        fblk=getFreeBlock();
        //printf("Shouldn be here <%d>\n",fblk );
        offset_blk=prev_blk->nxt_blk=malloc(sizeof(struct block_t));
        if(offset_blk == NULL)
        {
          perror("malloc");
          return errno;
        }
        prev_blk->nxt_blk->blk_num=fblk;
        prev_blk->nxt_blk->nxt_blk=NULL;
      }
      int dmove= (tmp_offset+buff_size)>(BLOCK_SIZE*offset_loop)?(BLOCK_SIZE*offset_loop - tmp_offset): buff_size;
      int recal_offset= tmp_offset - (BLOCK_SIZE*(offset_loop - 1));

      //printf("Will bewriting data as dmove <%d> recal_offset <%d> charcpy <%d>\n",dmove,recal_offset,charcpy);

      strncpy(memory_blocks[offset_blk->blk_num] + recal_offset,(buf+ charcpy),dmove);
      charcpy+=dmove;
      buff_size-=dmove;
      tmp_offset=BLOCK_SIZE*offset_loop;
      offset_loop++;
      prev_blk=offset_blk;
      offset_blk=offset_blk->nxt_blk;
    }
  }

  /*
  if(dirNode->data!=NULL)
    	len=dirNode->len;
  int mlc_chk=0;
	if (offset + size > len) {
		newSize=offset + size;
		if(len==0){
			mlc_chk=ckmalloc_w(newSize+1,&(dirNode->data));
      dirNode->len=newSize+1;
      if(mlc_chk!=0)
        return mlc_chk;
		}
		else{

       if(malloc_limit < (malloc_counter+(newSize-len)+1))
       {
          return -ENOSPC;
       }
			char *p=realloc(dirNode->data,newSize+1);
      if(p==NULL)
        return errno;
      dirNode->data=p;
			dirNode->data[newSize]='\0';
      dirNode->len=newSize+1;
      malloc_counter+=(newSize-len+1);
		}		
	}
	//printf("dir data :%s\n",dirNode->data);
	strncpy(dirNode->data + offset,buf, size);
	//printf(">>>> after dir data :%s\n",dirNode->data);

  */
  dirNode->len=(size+offset) > dirNode->len? (size+offset):dirNode->len;
  dirNode->access_time = time(NULL);

  return size;
}


// file rename 
static int rmfs_rename(const char *path,const char *path1)
{
  Node dirNode=NULL;
  char b[FILENAME_SIZE];
  memset(b,'\0',FILENAME_SIZE);
  getFileName(path1,b);

  int errChk=directory_lookup(path,&dirNode,0);
  if(errChk!=0) { 
      return -ENOENT; }
  strncpy(dirNode->name,b,sizeof(b));
  dirNode->access_time = time(NULL);
  return 0;
}

// This functiona is library defined , used for mapping which above functions to call with corresponding operation
static struct fuse_operations rmfs_oper = {
	.getattr	= rmfs_getattr,
	.access		= rmfs_access,
	.readdir	= rmfs_readdir,
	.open		= rmfs_open,
	.read		= rmfs_read,
	.mkdir		= rmfs_mkdir,
	.mknod		= rmfs_mknod,
	.flush 		= rmfs_flush,
	.create     = rmfs_create,
	.truncate   = rmfs_truncate,
	.ftruncate  = rmfs_ftruncate,
	.chmod      = rmfs_chmod,
	.chown      = rmfs_chown,
	.utimens    = rmfs_utimens,
	.unlink     = rmfs_unlink,
	.rmdir      = rmfs_rmdir,
	.write 		= rmfs_write,
  .rename   = rmfs_rename,
};

// For testing purpose .. creates list of files and directories
/*
int  makeSamplefile()
{
    int mlc_chk=0;
    strncpy(root->name,"/",strlen("/"));
    root->type=Ndir;
    root->next=NULL;

    Node first;
    mlc_chk=ckmalloc(sizeof(*root),&first);
    if(mlc_chk!=0)
      return mlc_chk;
    strncpy(first->name,"rmfs",strlen("rmfs"));
    first->type=Nfile;
    first->len=sizeof(char)*30;
    first->data=malloc(sizeof(char)*30);
    strncpy(first->data,"This is rmfs\n",strlen("This is rmfs\n"));

    Node sec;
    mlc_chk=ckmalloc(sizeof(*root),&sec);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(sec->name,"mello",strlen("mello"));
    sec->type=Nfile;
    sec->len=sizeof(char)*50;
    sec->data=malloc(sizeof(char)*50);
    strncpy(sec->data,"This is mello\n",strlen("This is mello\n"));

    Node third;
    mlc_chk=ckmalloc(sizeof(*root),&third);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(third->name,"extra",strlen("extra"));
    third->type=Ndir;

    Node third_ch;
    mlc_chk=ckmalloc(sizeof(*root),&third_ch);
    if(mlc_chk!=0)
      return ENOMEM;
    strncpy(third_ch->name,"file1",strlen("file1"));
    third_ch->type=Nfile;
    third_ch->len=sizeof(char)*30;
    third_ch->data=malloc(sizeof(char)*30);
    strncpy(third_ch->data,"This is file1\n",strlen("This is file1\n"));

    root->child=first;
    first->next=sec;
    first->child=NULL;
    sec->next=third;
    sec->child=NULL;
    third->child=third_ch;
    third_ch->next=NULL;

    return 0;
}

// To make FS persistent , during the umount creates serialize file for the metadata information and data stored inside the file 
// in the output directory specified at command line
void writeToFile(FILE **p,char pre[FULLPATHNAME],Node t)
{

  if(t==NULL)
    return;

   //printf(" pre : %s node name : %s\n",pre,t->name );
   char finalname[FULLPATHNAME];
   memset(finalname,'\0',FULLPATHNAME);
    //strcat(prefix, src);
   strncpy(finalname, pre,strlen(pre));
   if(strcmp(pre,"/")!=0)
    strcat(finalname, "/");
   strcat(finalname, t->name);

  //printf("inside the writeToFile <%s>\n",finalname);
    if(t->type==Nfile) {
     fprintf(*p, "%d|%s|%ld|\n",0,finalname,(t->data==NULL ? strlen("EMPTY\n"):strlen(t->data)));
     fprintf(*p, "%s",(t->data==NULL ? "EMPTY\n" : t->data));
   }
   else
   {
     fprintf(*p, "%d|%s|\n",1,finalname);
   }

   if(t->child != NULL)
    writeToFile(p,finalname,t->child);

   if(t->next != NULL)
    writeToFile(p,pre,t->next);

   return;
}

// While mountign ramfs it checks the persisten file exists or not and restore the file system as earlier.
void usePersistentFile(FILE **fp)
{

  while (1) 
  {
    ssize_t read;
    char * line = NULL;
    size_t len = 0;
    mode_t mode=0;
    int status=0;
    struct fuse_file_info *fi=NULL;

    if((read = getline(&line, &len, *fp)) == -1)
    {
       break;
    }

    //printf("Retrieved line of length %zu :\n", read);
    //printf("%s", line);
    long numr;
    char *token=strtok(line, "|");           
    int isfile=atoi(token);
    //printf(" isfile :%d\n",isfile );     
    token = strtok(NULL, "|");
    //printf("filename <%s>\n",token);
    if(isfile ==0)
    {
        char fullfilepath[1000];
        memset(fullfilepath,'\0',1000);
        strncpy(fullfilepath,token,strlen(token));
      //  printf("fullfilepath <%s>\n",fullfilepath);
        token = strtok(NULL, "|");
        long filelen=atol(token);
      //  printf("filelen <%ld>\n",filelen);
        char* buffer=malloc(filelen);
        memset(buffer,'\0',filelen);
        if((numr=fread(buffer,1,filelen,*fp))!=filelen){
            if(ferror(*fp)!=0){
                fprintf(stderr,"read file error.\n");
                exit(1);
            }
        }
       // printf(" buffer :<%s>\n",buffer );
        status=rmfs_create(fullfilepath, mode,fi);
        if(status !=0)
          break;
        if(strcmp(buffer,"EMPTY\n")!=0)
	{
          status=rmfs_write(fullfilepath, buffer, filelen,0, fi);
      	  if(status !=filelen)
            break;
	}
        free(buffer);
    }
    else
    {
      status=rmfs_mkdir(token,mode);
      if(status !=0)
          break;
    }

    if (line)
       free(line);
  }
}

*/
// Main function takes arguments as mentioned ..
// Mount point -  where FS should be mounted 
// Size :  size of fs as whole
// Restore path :  extra handling to make file system persistent and to restored from with specified directory
int main(int argc, char *argv[])
{
  int eflag=0,pstr=0,i;
  FILE * fp;
  pthread_t thread1, thread2;
  int ret1, ret2;
  

  if(argc < 3 || argc >4)
  {
    printf("Usage : ./ramdisk <mount_point> <size in MB> <Restore Filepath> \n");
    exit(-1);
  }

  malloc_limit=atoi(argv[2]);
  printf("Malloc limit <%d>\n",malloc_limit);
  // Setting up block allocation for the in-memory file system
  free_block_count=block_count=(malloc_limit*MB_CONVERT)/BLOCK_SIZE; 
  memory_blocks = malloc(block_count * sizeof(char*));
  if ( memory_blocks == NULL ) {
      perror("malloc");
      return errno;
  }
   printf("after block count malloc  \n");
  for(i=0;i<block_count; i++)
  {
    memory_blocks[i]=malloc(BLOCK_SIZE * sizeof(char));

    if ( memory_blocks[i] == NULL ) {
      perror("malloc");
      return errno;
    }
    memset(memory_blocks[i],'\0',BLOCK_SIZE);
  }

  printf("before free block count malloc bloc count <%d> \n",block_count);
  // Initialize the free block structure
  free_blk=malloc(block_count * sizeof(int));
  if ( free_blk == NULL ) {
      perror("malloc");
      return errno;
  }

  for(i=0;i<block_count; i++)
  {
    free_blk[i]=-1;
  }  

  // argv[2]="-d";

  if(argc == 4)
    pstr=1;
  //argc=3;
  argc=2;
  //defining the root element

  printf("D1\n");

  int mlc_chk=ckmalloc(sizeof(*root),&root);
  if(mlc_chk!=0)
      return mlc_chk;

  printf("D2\n");
  strncpy(root->name,"/",strlen("/"));
  root->type=Ndir;
  root->next=root->child=NULL;
  root->data=NULL;

  printf("D3\n");  
  if(pstr == 1)
  {    
   printf("D4\n"); 
    fp = fopen (argv[3], "a+");
    if (fp == NULL)
      exit(EXIT_FAILURE);
    // call to restore the path
   // usePersistentFile(&fp);
    fclose(fp);
  }
  //  eflag=makeSamplefile();

  printf("before the fuse main \n");
  /*multithread to keep track of cold files*/
	ret1 = pthread_create ( &thread1, NULL, track_cold_files, NULL);
	if(ret1) {
		printf(stderr, "Pthread create failed\n");
		exit(1);
   }	

  eflag=fuse_main(argc, argv, &rmfs_oper, NULL);

  if(pstr == 1)
  {
    char prefix[1000]="/";
    fp = fopen (argv[3], "w+");
    if (fp == NULL)
       exit(EXIT_FAILURE);

    // Make FS persistent
  //  writeToFile(&fp,prefix,root->child);
   
    fclose(fp);
  }
  
  pthread_join(thread1, NULL);
  return eflag;
}

