#include "storage.h"

time_t timer = NULL;
char buffer[26] = {NULL};
struct tm* tm_info = NULL;

void printTime(){
	
	time(&timer);
	tm_info = localtime(&timer);
	
	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
    	//printf("%s%s ",KRED, buffer);
	printf(KRED "[%s] " RESET, buffer);
}

/************************Dropbox API code*********************/
int download_dropbox_file(char *, char *, char *);
drbClient * intialize_drop_box(){
	
	char *c_key    = "e9kwa4b6lnxk0r5";  //< consumer key
	char *c_secret = "sr8lt6xj5izjtol";  //< consumer secret
	//char *t_key    = "KSkQCyhUYNIAAAAAAAAACdaOg1cDsSD4GBI_Q_I7ajkC5zhzBCZnk00rtMrfmhbf"; //< access token key
	char *t_key = "2vrs94p0v2evy90a";
	char *t_secret = "b33n459lg42bzom";  //< access token secret
	drbInit();
	drbClient* cli_local = drbCreateClient(c_key, c_secret, t_key, t_secret);
	connect_to_dropbox(cli_local, t_key, t_secret);
	drbSetDefault(cli_local, DRBOPT_ROOT, DRBVAL_ROOT_AUTO, DRBOPT_END);
	
	return cli_local;
}

void move_to_dropbox_cold_storage(char * fileName){
	
	char DRBFilePath[255];
	char path[255];
	
	strcpy(path,"./");
	strcat(path, fileName);
	strcpy(DRBFilePath, "/");
	strcat(DRBFilePath, fileName);
	
	//printf("File name: %s \n", fileName);
	//printf("Local file path: %s \n", path);
	//printf("Dropbox file path: %s \n", DRBFilePath);
	
	upload_dropbox_file(path, fileName, DRBFilePath);
}

void fetch_from_dropbox_cold_storage(char * fileName){
	
	char DRBFilePath[255];
	char path[255];
	
	strcpy(path,"./");
	strcat(path, fileName);
	strcpy(DRBFilePath, "/");
	strcat(DRBFilePath, fileName);
	
	download_dropbox_file(path, fileName, DRBFilePath);
}

void upload_dropbox_file(char *localFilePath, char *fileName, char *DRBFilePath){
	
	int err;
	FILE *file = fopen(localFilePath, "r");
	T(printf("Uploading Block %s...\n", fileName));
	err = drbPutFile(cli, NULL,
	DRBOPT_PATH, DRBFilePath,
	DRBOPT_IO_DATA, file,
	DRBOPT_IO_FUNC, fread,
	DRBOPT_END);
	fclose(file);
	T(printf("Block upload: %s\n", err ? "Failed" : "Successful"));
	remove(localFilePath);
}

int download_dropbox_file(char *localFilePath, char *fileName, char *DRBFilePath){
	
	int err;
	FILE *file = fopen(localFilePath, "w"); // Write it in this file
	void* output = NULL;
	T(printf("Downloading Block %s...\n", fileName));
	err = drbGetFile(cli, &output,
	DRBOPT_PATH, DRBFilePath,
	DRBOPT_IO_DATA, file,
	DRBOPT_IO_FUNC, fwrite,
	DRBOPT_END);
	fclose(file);
	T(printf("Block Download: %s\n", err ? "Failed" : "Successful"));
	return err;
}

int connect_to_dropbox(drbClient* cli_local, char *t_key, char *t_secret){
	// Request a AccessToken if undefined (NULL)
	if(!t_key || !t_secret) {
		drbOAuthToken* reqTok = drbObtainRequestToken(cli_local);
		
		if (reqTok) {
			char* url = drbBuildAuthorizeUrl(reqTok);
			printf("Please visit %s\nThen press Enter...\n", url);
			free(url);
			fgetc(stdin);
			
			drbOAuthToken* accTok = drbObtainAccessToken(cli_local);
			
			if (accTok) {
				// This key and secret can replace the NULL value in t_key and
				// t_secret for the next time.
				printf("key:    %s\nsecret: %s\n", accTok->key, accTok->secret);
			} else{
				fprintf(stderr, "Failed to obtain an AccessToken...\n");
				return EXIT_FAILURE;
			}
		} else {
			fprintf(stderr, "Failed to obtain a RequestToken...\n");
			return EXIT_FAILURE;
		}
	}
	return 0;
}
/*************************************************************/

//***********************WRITE TO COLD STORAGE****************/
char * hash_calc(Block);
void print_cold_blocks(Block);
drbClient * intialize_drop_box();
gboolean hashtree_contains(char *);
/************************Code for Accessing Blocks**************/
/*void check_new_hashes(Node cold_file){
Block cold_block = cold_file->data;

char *current_hash;

while(cold_block != NULL)
{
current_hash = cold_block->server_block_hash;
printf("\n\nHash under consideration: %s\n\n",current_hash);
cold_block = cold_block->nxt_blk;
}
}*/
/************************Code for Accessing Blocks**************/
void write_access_cold_blocks(Node cold_file){
	
	Block cold_block = cold_file->data;
	char *hash;

	D(printf("(File: %s) Transferring to cold storage.\n",cold_file->name));

	while(cold_block != NULL)
	{
		if(difftime(latest_file_access_time,cold_file->access_time) >= 0){
			hash = hash_calc(cold_block);
			
			if(!hashtree_contains(hash)){
				write_block_to_file(cold_block,hash);
				move_to_dropbox_cold_storage(hash);
				update_hashtree(hash);

				T(printf("(Block: %ld) Successfully Transferred to cold storage.\n",cold_block->blk_num));
				T(printf("------------------------------------------------------\n"));
			}else
			{
				T(printf("(Block: %ld) Already on cold storage.\n",cold_block->blk_num));
				T(printf("------------------------------------------------------\n"));
			}
			
			cold_block->server_block_hash = hash;
			cold_block->inmemory_flag = False;
			
			//print_cold_blocks(cold_block);
			cold_block = cold_block->nxt_blk;
		}
		else{
			D(printf("(Cold File: %s) Accessed just now! \nBreaking File Transfer. Cold File Access Time: %ld, Benchmark Time: %ld\n",cold_file->name,cold_file->access_time,latest_file_access_time));

			return;
		}
	}
	D(printf("(File: %s) Successfully transferred to cold storage.\n",cold_file->name));	
	
	// Freeing all the transfered blocks for host storage.
	pthread_mutex_lock(&(cold_file->lock));
	cold_block = cold_file->data;
	
	while(cold_block != NULL){
		T(printf("(Block: %ld) Freeing memory.\n",cold_block->blk_num));
		free_blk[cold_block->blk_num] = -1;
		memset(memory_blocks[cold_block->blk_num], '\0',BLOCK_SIZE);
		pthread_mutex_lock(&count_lock);
		free_block_count++;
		pthread_mutex_unlock(&count_lock);
		cold_block = cold_block->nxt_blk;
	}
	D(printf("(File: %s) All Blocks freed.\n",cold_file->name));	
	D(printf("------------------------------------------------------\n"));
	
	cold_file->inmemory_node_flag=False;
	
	pthread_mutex_unlock(&(cold_file->lock));
	//check_new_hashes(cold_file);
}

void print_cold_blocks(Block cold_block){
	printf("\nAfter Changing Data Block \"%d\" and data: \"%s\" \n",cold_block->blk_num, cold_block->server_block_hash);
}

/***********************Code for Finding Hash for a Block***************/
char * hash_calc(Block cold_block)
{
	T(printf("(Block: %ld) Calculating Hash.\n",cold_block->blk_num));
	int i;
	char *buf;
	buf = (char *) malloc(sizeof(char)*SHA_DIGEST_LENGTH*2 + 1);
	unsigned char hash[SHA_DIGEST_LENGTH];
	
	memset(buf, 0x0,SHA_DIGEST_LENGTH*2);
	
	SHA1(memory_blocks[cold_block->blk_num], BLOCK_SIZE, hash);
	
	for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf((char*)&(buf[i*2]), "%02x", hash[i]);
	}
	buf[SHA_DIGEST_LENGTH*2]='\0';

	T(printf("(Block: %ld) Hash Calculated.\n",cold_block->blk_num));	
	
	return buf;
}

/***********************Code for Getting Server Side Hashtree (hashOfBlock -> BlockNames)***************/
void iterator(gpointer key, gpointer value, gpointer user_data) {
	printf(user_data, key, value);
}

void create_hashtree(){
	
	char hash[40];
	char block_name[20];
	char linebuff[61];
	hashtree = g_hash_table_new(g_str_hash, g_str_equal);
	block_number = 0;
	
	FILE *hash_file;
	hash_file= fopen("./dropbox_hashtree.txt", "r");
	
	while(fgets(linebuff, sizeof(linebuff), hash_file)){
		sscanf(linebuff,"%s %s",hash,block_name);
		g_hash_table_insert(hashtree,  g_strdup(hash),  g_strdup(block_name));
		
		block_number++;
	}
	
	fclose(hash_file);
}

void activate_hashtree(){
	
	T(printf("Fetching Hashtree.\n"));
	int hashtree_file_exist = download_dropbox_file("./dropbox_hashtree.txt", "dropbox_hashtree.txt", "/dropbox_hashtree.txt");

	if(hashtree_file_exist == 0){
		create_hashtree();
		T(printf("Hashtree Fetched, Creating 	inmemory Hashtree.\n"));
	}
	else{
		T(printf("Hashtree does not exist on cold storage, Creating a new inmemory Hashtree.\n"));
		hashtree = g_hash_table_new(g_str_hash, g_str_equal);
	}
}

/***********************Code for Identifying duplicate block in Server Side Hashtree***************/
gboolean hashtree_contains(char *hash){
	
	gboolean contains = g_hash_table_contains(hashtree, hash);
	
	return contains;
}

/***********************Code for Changing each non-duplicate block to a file (block_1, block_2)***************/
void write_block_to_file(Block cold_block, char * file_name){
	
	FILE *fp;
	int file_write_result;
	
	char cold_block_path[80];
	memset(cold_block_path,'\0',80);
	strcpy(cold_block_path, "./");
	strcat(cold_block_path, file_name);
	
	fp=fopen(cold_block_path, "wb");
	//printf("Read Content: %s", memory_blocks[cold_block->blk_num]);
	file_write_result = fwrite(memory_blocks[cold_block->blk_num],BLOCK_SIZE,1,fp);
	//file_write_result = fprintf(fp,"%s", memory_blocks[cold_block->blk_num]);
	//printf("\nFile Write Result : %d", file_write_result);
	
	fclose(fp);
}

/***********************Code for updating server side Hashtree***************/
void update_hashtree(char *hash){
	
	T(printf("Updating Hashtree (Inmemory and File).\n"));
	
	char block_name[20];
	sprintf(block_name, "block_%d", block_number);
	block_number++;
	
	g_hash_table_insert(hashtree, hash, block_name);
	
	FILE * file= fopen("./dropbox_hashtree.txt", "a");
	if (file != NULL) {
		fprintf(file, "%s %s\n", hash, block_name);
		fclose(file);
	}
}

/***********************Code for Updating client Side Hashtree of Blocks(File -> hashofBlocks): Not Required***************/

/***********************READ FROM COLD STORAGE***************/
/***********************Code for Using client Side Hashtree of Blocks(File -> hashofBlocks)***************/
/***********************Code for Getting Server Side Hashtree (hashOfBlock -> BlockNames: Already Implemented Above)***************/
/***********************Code for Identifying Block Files to be copied from Server***************/
/***********************Code for Writing Block File data to blocks on client***************/
void read_block_from_file(char * file_name, Block cold_block){
	
	FILE *fp;
	char cold_block_path[80];
	char *string = malloc(BLOCK_SIZE + 1);
	
	strcpy(cold_block_path, "./");
	strcat(cold_block_path, file_name);
	
	fp=fopen(cold_block_path, "rb");
	fread(string, BLOCK_SIZE, 1, fp);
	
	fclose(fp);
	//printf("Fetched String: %s",string);
	
	long fblk = getFreeBlock();
	cold_block -> blk_num =fblk;
	strncpy(memory_blocks[fblk], string, strlen(string));
	
	remove(file_name);
}

void read_access_cold_blocks(Node cold_file){
	
	Block cold_block = cold_file->data;
	char *hash;
	int hashtree_activated = 0;
	
	/**********Move this to calling function*********************/
	if(thread_flag != 1){
		activate_hashtree();
		hashtree_activated = 1;
	}
	else{
		D(printf("(File: %s) Another Thread Running. Using existing hashtree. \n",cold_file->name));	
	}

	D(printf("(File: %s) Retrieving from cold storage.\n",cold_file->name));
	
	while(cold_block != NULL)
	{
		hash = cold_block->server_block_hash;
		
		if(hashtree_contains(hash)){
			fetch_from_dropbox_cold_storage(hash);
			read_block_from_file(hash, cold_block);
			T(printf("(Block: %ld) Retrieved from cold storage.\n",cold_block->blk_num));
			T(printf("------------------------------------------------------\n"));
		}else
		{
			D(printf("(Block: %ld) No found on cold storage.\n",cold_block->blk_num));
			T(printf("------------------------------------------------------\n"));
		}
		
		cold_block->server_block_hash = NULL;
		cold_block->inmemory_flag = True;
		
		//print_cold_blocks(cold_block);
		cold_block = cold_block->nxt_blk;
	}

	D(printf("(File: %s) Retrieved from cold storage.\n",cold_file->name));
	D(printf("------------------------------------------------------\n"));
	/**********Move this to calling function*********************/
	cold_file->inmemory_node_flag = True;
	if(hashtree_activated == 1){
		remove("./dropbox_hashtree.txt");
	}
}
