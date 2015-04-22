/*
Author : Aditya Ambre, Madhura S. Raghavan, Rohit Arora
Fuse based File system which supports POSIX functionalities.

FUSE: Filesystem in Userspace
Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

This program can be distributed under the terms of the GNU GPL.
See the file COPYING.

make unifiedFS
*/

#include "storage.h"

// List for access and retrival of files.
List_item *acclist_head = NULL;
List_item *transfer_list = NULL;
List_item *rtvlist_head = NULL;

// Memory Counter variables
long long malloc_counter=0,malloc_limit=0,block_count=0, free_block_count=0;
int block_number;

// defining root node here
Node root,buffNode;
char **memory_blocks=NULL;

// Array maintaining free blocks
int *free_blk=NULL;

// Flags
int thread_flag = 0;

// Benchmark Time
time_t latest_file_access_time;

// Locks
pthread_mutex_t count_lock;

// Dropboc Client and HashTree Data Structure
drbClient *cli;
GHashTable* hashtree;

int getFreeBlock()
{
	int i;
	
	for(i=0;i<block_count;i++)
	{
		if(free_blk[i]==-1)
		{
			free_blk[i]=0;
			pthread_mutex_lock(&count_lock);
			free_block_count--;
			pthread_mutex_unlock(&count_lock);
			return i;
		}
	}
	
	printf(" running out of memory \n");
	exit(1);
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
	blk=NULL;
	pthread_mutex_lock(&count_lock);
	free_block_count++;
	pthread_mutex_unlock(&count_lock);
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
	n=NULL;
	//malloc_counter-=totalsize;
}


//
void freeColdBlock(Block blk)
{
	if(blk==NULL)
		return;
	freeColdBlock(blk->nxt_blk);
	//free_blk[blk->blk_num]=-1;
	//memset(memory_blocks[blk->blk_num],'\0',BLOCK_SIZE);
	free(blk);
	blk=NULL;
	
}

// Freemalloc function for cold files
void freemallocColdFiles(Node n)
{
	long totalsize=0;
	if(n->data!=NULL)
	{
		totalsize+=n->len;
		freeColdBlock(n->data);
	}
	totalsize+=sizeof(*root);
	free(n);
	n=NULL;
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
	pthread_mutex_lock(&(temp->lock));
	pthread_mutex_unlock(&(temp->lock));
	*t=temp;
	return 0;
}

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
			if(temp->inmemory_node_flag == False){
				stbuf->st_mode = S_IFREG | 1666;
			}
			else{
				stbuf->st_mode = S_IFREG | 0666;
			}
			stbuf->st_size = 0;
			if(temp->type==Ndir)
			stbuf->st_mode = S_IFDIR | 0666;
			else if(temp->data !=NULL)
			stbuf->st_size=temp->len;
			stbuf->st_nlink = 1;
			stbuf->st_atime=temp->access_time;
			stbuf->st_mtime=temp->access_time;
		}
		else
			res = -ENOENT;
	}
	buffNode=temp;
	//printf("<<<<<<< %lld  , %lld>>>>>>>\n",malloc_counter,malloc_limit );
	printf(" >>>>>>>>>>>>Used Blocks is %ld, Block count is %ld and CheckColdStorage \n", block_count-free_block_count, block_count);
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
	int ret = 0;
	char lpath[100];
	memset(lpath,'\0',100);
	pthread_t thread;
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
	
	pthread_mutex_lock(&(dirNode->lock));
	pthread_mutex_unlock(&(dirNode->lock));
	
	if(dirNode->inmemory_node_flag == False)
	read_access_cold_blocks(dirNode);
	
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
	
	if((checkStorageThreshold(MAX_STORAGE_THRESHOLD)) && (thread_flag == 0)) {
		/*multithread to keep track of cold files*/
		thread_flag = 1;
		latest_file_access_time = time(NULL);
		ret = pthread_create ( &thread, NULL, track_cold_files, NULL);
		if(ret) {
			printf("Pthread create failed\n");
			exit(1);
		}
	}
	
	
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
	/****Making Node Level Inmemory Flag True***********************************************/
	newDirNode->inmemory_node_flag = True;
	
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
	pthread_t thread;
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
	
	if(temp->inmemory_node_flag == True)
	  freemalloc(temp);
	else
 	  freemallocColdFiles(temp);
	
	temp=NULL;
	
	if((checkMinStorageThreshold(MIN_STORAGE_THRESHOLD)) && (thread_flag == 0))
	{
		thread_flag = 1;
		int ret = pthread_create ( &thread, NULL, get_cold_files, NULL);
		if(ret) {
			printf("Pthread create failed\n");
			exit(1);
		}
	}
	
	return 0;
}

// Remove directory
static int rmfs_rmdir(const char *path)
{
	char lpath[100];
	memset(lpath,'\0',100);
	pthread_t thread;
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
	temp=NULL;
	
	if((checkMinStorageThreshold(MIN_STORAGE_THRESHOLD)) && (thread_flag == 0))
	{
		thread_flag = 1;
		int ret = pthread_create ( &thread, NULL, get_cold_files, NULL);
		if(ret) {
			printf("Pthread create failed\n");
			exit(1);
		}
	}
	
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
	pthread_t thread;
	
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
	pthread_t thread;
	int ret;
	
	
	char lpath[100];
	memset(lpath,'\0',100);
	memcpy(lpath,path,strlen(path));
	
	//printf("$$$$$$$$$$$$ Write Path:<%s>\n Contents: <%s>\n", lpath,buf);
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
	
	pthread_mutex_lock(&(dirNode->lock));
	pthread_mutex_unlock(&(dirNode->lock));
	
	if(dirNode->inmemory_node_flag == False)
	read_access_cold_blocks(dirNode);
	
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
		/****Making Block Level Inmemory Flag True***********************************************/
		dirNode->data->inmemory_flag=True;
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
				/****Making Block Level Inmemory Flag True***********************************************/
				prev_blk->nxt_blk->inmemory_flag=True;
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
	
	dirNode->len=(size+offset) > dirNode->len? (size+offset):dirNode->len;
	dirNode->access_time = time(NULL);
	//write_access_cold_blocks(dirNode);
	
	if((checkStorageThreshold(MAX_STORAGE_THRESHOLD)) && (thread_flag == 0)) {
		/*multithread to keep track of cold files*/
		thread_flag = 1;
		latest_file_access_time = time(NULL);
		ret = pthread_create ( &thread, NULL, track_cold_files, NULL);
		if(ret) {
			printf("Pthread create failed\n");
			exit(1);
		}
	}
	
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

// Main function takes arguments as mentioned ..
// Mount point -  where FS should be mounted
// Size :  size of fs as whole
// Restore path :  extra handling to make file system persistent and to restored from with specified directory
int main(int argc, char *argv[])
{
	int eflag=0,pstr=0,i;
	FILE * fp;
	
	
	if(argc < 3 || argc >4)
	{
		printf("Usage : ./ramdisk <mount_point> <size in MB> <Restore Filepath> \n");
		exit(-1);
	}
	//Initialize the global lock
	if (pthread_mutex_init(&count_lock, NULL) != 0)
	{
		printf("\n mutex init failed\n");
		return 1;
	}
	
	
	cli = intialize_drop_box();
	printf("Cli: %s", cli);
	block_number = 0;
	
	malloc_limit=atoi(argv[2]);
	printf("Malloc limit <%ld>\n",malloc_limit);
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
	
	printf("before free block count malloc bloc count <%ld> \n",block_count);
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
	
	argv[2]="-d";
	
	if(argc == 4)
	pstr=1;
	argc=3;
	//argc=2;
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
	eflag=fuse_main(argc, argv, &rmfs_oper, NULL);
	
	printf("After fuse main\n");
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
	pthread_mutex_destroy(&count_lock);
	//pthread_join(thread, NULL);
	return eflag;
}
