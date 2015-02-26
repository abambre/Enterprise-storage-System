#ifndef dll_H
#define dll_H

#include<stdio.h>
#include<stdlib.h>

#define SUCCESS 0
#define FAILED -1

/*Double linked list node structure*/
typedef struct List_Node 
{
	void *data;
    struct List_Node *next;
    struct List_Node *prev;
}List_Node;

/*Doubly linked list structure*/
typedef struct dList
{
	List_Node *head;
    List_Node *tail;
	int size;
}dList;

/*Intialize the DLL*/
int intialize_dList(dList *list) {
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
}	

/*Insert to the end of the List*/
int insert_back(void *data,dList *list) {
	List_Node *node = NULL;
	
	dnode = (List_Node*)malloc(sizeof(List_Node));
	if(NULL == dnode) {
		printf("Malloc Error\n");
		exit(1);
	}
	dnode->next = dnode->prev = NULL;
	dnode->data = data;
	 
	/*Check if the List is empty - Insert fist Node*/
	if(list->head == NULL && list->tail == NULL && list->size == 0) {
		list->head = list->tail = dnode;
		list->size++;		
		retrun SUCCESS;
	}
	
	/*Only one Node or General case insert at back*/
	else {
		list->tail->next = dnode;
		dnode->prev = list->tail;
		list->tail = dnode;	
		list->size++;
		return SUCCESS;
	}
	return FAILED;
}	

void *delete_front(dList *list) {
	List_Node *temp = NULL;
	void *ret = NULL;
	if(list->head == NULL and list->tail == NULL) {
		return NULL;
	}	

	/*only one node*/
	if(list->head == list->tail) {
		ret = list->head->data;
		free(list->head);
		list->head = list->tail = NULL;
		list->size = 0;
		return ret;
	}
	else {     			/*Many Nodes - Delete front*/
		ret = list->head->data;
		temp = list->head;
		list->head = list->head->next;
		list->size--;
		free(temp);
		return ret;
	}
}
					
#endif
