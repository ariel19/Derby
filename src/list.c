#include "list.h"
#include <stdlib.h>

void list_init(struct list_user *plu) {
	plu->head = plu->tail = NULL;
}

void list_add_end(struct list_user *plu, struct user *pu) {
	struct user_node *user_node = (struct user_node *)malloc(sizeof(struct user_node));	
	user_node->user = pu;
	user_node->next = NULL;
	if (!plu->head) {
		plu->head = plu->tail = user_node;
		return;
	}
	/*
	if (plh->head == plh->tail) {
		plh->tail = ph;
		plh->head->next = plh->tail;
		return;
	}
	*/
	plu->tail->next = user_node;
	plu->tail = user_node;
}

void list_clean(struct list_user *plu) {
	struct user_node *ptr = plu->head,
			*nxt = NULL;
	while (ptr) {
		nxt = ptr->next;
		free(ptr);
		ptr = nxt;
	}
	plu->head = plu->tail = NULL;
}


