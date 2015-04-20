#include "storage.h"

int checkStorageThreshold(int percent)
{
	int ret = 0;
	ret = ((float)((float)(block_count-free_block_count)/(float)block_count)*100) > percent ? 1:0;
	printf("Inside the checkStorageThreshold <%f> \n",(((float)(block_count-free_block_count)/(float)block_count)*100));
	printf("Percent is %d, Used Blocks is %ld, Block count is %ld and CheckColdStorage ret is %d\n", percent, block_count-free_block_count, block_count, ret);
	return ret;
}

/*function to insert a Node to the back of the list specified with head*/
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

/*Remove from front of List - Least accessed File/Cold File specified by head*/
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

/*Sort the list according to Size*/
List_item *sort_list_size(List_item *lroot) {
	int size;
	List_item *curr = NULL, *highest = NULL, *highest_prev = NULL, *prev = NULL;
	List_item *tmp = NULL;
	
	if ( (lroot == NULL) || (lroot->next == NULL) )
		return lroot;
	
	curr = highest = highest_prev = prev = lroot;
	while(curr != NULL) {
		size = curr->inode->len - highest->inode->len;
		if (size > 0) {
			highest_prev = prev;
			highest = curr;
		}
		else if(size == 0) {
			if(curr->inode->access_time < highest->inode->access_time) {
				highest_prev = prev;
				highest = curr;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	
	if(highest != lroot) {
		highest_prev->next = lroot;
		tmp = lroot->next;
		lroot->next = highest->next;
		highest->next = tmp;
	}
	
	highest->next = sort_list_size(highest->next);
	return highest;
}


void print_list(List_item *list) {
	List_item *temp = NULL;
	if (list == NULL) {
		printf("List head empty\n");
		return;
	}
	/*if (*acclist_head == NULL) {
	printf("List is empty\n");
	return;
	}*/
	temp = list;
	while(temp != NULL) {
		printf("%s:%ld->",temp->inode->name, temp->inode->access_time);
		temp = temp->next;
	}
}

int populate_access_list() {
	Node temp = root;
	List_item *fs_head = NULL;
	int count = 0;
	
	if ((root->child == NULL) && (root->next == NULL)) return 0;
	insert_back(root, &fs_head);
	
	while(fs_head != NULL) {
		temp = get_inode(&fs_head);
		temp = temp->child;
		while (temp != NULL) {
			if(temp->type == Ndir) {
				insert_back(temp, &fs_head);
			}
			else if(temp->type == Nfile) {
				if(temp->inmemory_node_flag == True) {
					insert_back(temp, &acclist_head);
					count++;
				}
			}
			temp = temp->next;
		}
	}
	return count;
}

long calc_blcks_transfer(int low_thresh) {
	long num_blks_for_lowthresh = 0;
	long blks_to_transfer = 0;
	num_blks_for_lowthresh = (low_thresh * block_count) / 100 ;
	blks_to_transfer = (block_count-free_block_count) - num_blks_for_lowthresh;
	return blks_to_transfer;
}

int calc_file_blks(int file_len) {
	return ((file_len + (BLOCK_SIZE-1)) / BLOCK_SIZE);
}

void prepare_nodelist_to_transfer() {
	long num_blks_to_transfer = 0;
	long num_file_blks = 0;
	Node node_considered = NULL;
	
	num_blks_to_transfer = calc_blcks_transfer(OPTIMAL_STORAGE_THRESHOLD);
	//printf("\n########## Number Of blocks to transfer = %ld\n", num_blks_to_transfer);
	while(num_blks_to_transfer >= 1) {
		node_considered = get_inode(&acclist_head);
		if(node_considered == NULL) break;
		num_file_blks = calc_file_blks(node_considered->len);
		//printf("####Len of %s is %d and num_blocks is %ld\n", node_considered->name, node_considered->len,num_file_blks);
		insert_back(node_considered, &transfer_list);
		num_blks_to_transfer = num_blks_to_transfer - num_file_blks;
		//printf("Number of blks remaining to be transferred is %ld\n", num_blks_to_transfer);
	}
	//printf("\n Transfer List before sorting is :\n");
	print_list(transfer_list);
	transfer_list = sort_list_size(transfer_list);
}

/*-----------------Track cold Files---------------*/

void *track_cold_files() {
	
	int num_files;
	Node node_to_transfer = NULL;
	List_item *temp = NULL;
	
	printf("One Thread getting called\n");
	//sleep(1);
	num_files = populate_access_list();
	
	
	acclist_head = sort_list(acclist_head);
	printf("\n***Printing Sorted Access List***\n");
	print_list(acclist_head);
	
	/****************** New Changes******************/
	prepare_nodelist_to_transfer();
	printf("\n****Printing Access List based on Size***\n");
	print_list(transfer_list);
	
	/**********Move this to calling function*********************/
	activate_hashtree();
	
	/*Code to transfer Files*/
	while((transfer_list != NULL) && (checkStorageThreshold(OPTIMAL_STORAGE_THRESHOLD))) {   //Until the storage utilization drops to 40% continue giving noDe
		node_to_transfer = get_inode(&transfer_list);
		write_access_cold_blocks(node_to_transfer);
	}
	/**********Move this to calling function*********************/
	upload_dropbox_file("./dropbox_hashtree.txt", "dropbox_hashtree.txt", "/dropbox_hashtree.txt");
	/*if((acclist_head == NULL) && (checkStorageThreshold(1))) {
	printf("Storage full but access list empty - No more files to transfer\n");
	}*/
	printf("Freeing memory\n");
	while(acclist_head != NULL) {
		temp = acclist_head;
		acclist_head = acclist_head->next;
		free(temp);
	}
	temp = NULL;
	while(transfer_list	!= NULL) {
		temp = transfer_list;
		transfer_list = transfer_list->next;
		free(temp);
	}
	temp = NULL;
	printf("Exiting thread\n");
	thread_flag = 0;
	pthread_exit(NULL);
}

/*----------------Get cold files ------------------------*/

int checkMinStorageThreshold(int percent)
{
	int ret = 0;
	ret = ((float)((float)(block_count-free_block_count)/(float)block_count)*100) < percent ? 1:0;
	printf("Inside the checkMinStorageThreshold <%f> \n",(((float)(block_count-free_block_count)/(float)block_count)*100));
	printf("Percent is %d, Used Blocks is %ld, Block count is %ld and CheckColdStorage ret is %d\n", percent, block_count-free_block_count, block_count, ret);
	return ret;
}


int populate_retrieval_list() {
	Node temp = root;
	List_item *fs_head = NULL;
	int count = 0;
	
	if ((root->child == NULL) && (root->next == NULL)) return 0;
	insert_back(root, &fs_head);
	
	while(fs_head != NULL) {
		temp = get_inode(&fs_head);
		temp = temp->child;
		while (temp != NULL) {
			if(temp->type == Ndir) {
				insert_back(temp, &fs_head);
			}
			else if(temp->type == Nfile) {
				if(temp->inmemory_node_flag == False) {
					insert_back(temp, &rtvlist_head);
					count++;
				}
			}
			temp = temp->next;
		}
	}
	return count;
}

void print_retrieval_list() {
	List_item *temp = NULL;
	if (rtvlist_head == NULL) {
		printf("List head empty\n");
		return;
	}
	/*if (*rtvlist_head == NULL) {
	printf("List is empty\n");
	return;
	}*/
	temp = rtvlist_head;
	printf("Printing File retrieval list\n");
	while(temp != NULL) {
		printf("%s:%ld->",temp->inode->name, temp->inode->access_time);
		temp = temp->next;
	}
}

List_item *sort_rlist(List_item *lroot) {
	double seconds;
	List_item *curr = NULL, *latest = NULL, *latest_prev = NULL, *prev = NULL;
	List_item *tmp = NULL;
	
	if ( (lroot == NULL) || (lroot->next == NULL) )
		return lroot;
	
	curr = latest = latest_prev = prev = lroot;
	while(curr != NULL) {
		seconds = difftime(curr->inode->access_time,latest->inode->access_time);
		if (seconds > 0) {  // printf("Date1 > Date2\n");
			//if (curr->inode->retrieval_time < latest->inode->retrieval_time) {
			latest_prev = prev;
			latest = curr;
		}
		prev = curr;
		curr = curr->next;
	}
	
	if(latest != lroot) {
		latest_prev->next = lroot;
		tmp = lroot->next;
		lroot->next = latest->next;
		latest->next = tmp;
	}
	
	latest->next = sort_rlist(latest->next);
	return latest;
}

void *get_cold_files() {
	
	int num_files;
	Node node_to_retrive = NULL;
	List_item *temp = NULL;
	printf("get cold files thread getting called\n");
	//sleep(2);
	//List_item *rtvlist_head = NULL;
	
	num_files = populate_retrieval_list();
	print_retrieval_list();
	printf("\nDone printing\n");
	rtvlist_head = sort_rlist(rtvlist_head);
	printf("\nSorting done\n");
	print_retrieval_list();
	printf("\nSorted Printing  done\n");
	
	/*Code to retrive Files*/
	while((rtvlist_head != NULL) && (checkMinStorageThreshold(OPTIMAL_STORAGE_THRESHOLD))) {   //Until the storage utilization drops to 40% continue giving noDe
		node_to_retrive = get_inode(&rtvlist_head);
		read_access_cold_blocks(node_to_retrive);
	}
	/**********Move this to calling function*********************/
	//upload_dropbox_file("./dropbox_hashtree.txt", "dropbox_hashtree.txt", "/dropbox_hashtree.txt");
	/*if((rtvlist_head == NULL) && (checkStorageThreshold(1))) {
	printf("Storage full but retrieval list empty - No more files to retrive\n");
	}*/
	
	printf("Freeing memory\n");
	while(rtvlist_head != NULL) {
		temp = rtvlist_head;
		acclist_head = rtvlist_head->next;
		free(temp);
	}
	temp = NULL;
	
	printf("Exiting thread\n");
	thread_flag = 0;
	pthread_exit(NULL);
}
