/*
 * Linked list
 */

#ifndef TYPES_LIST_H
#define TYPES_LIST_H

/** List link */
typedef struct link {
	struct link *prev, *next;
} link_t;

/** Doubly linked list */
typedef struct {
	link_t head;
} list_t;

#endif
