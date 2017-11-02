/*
 * Linked list
 */

#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <types/adt/list.h>

#define list_get_instance(link, type, member) \
	((type *)( (char *)(link) - ((char *) &((type *) NULL)->member)))

#define list_foreach(list, member, itype, iterator) \
	for (itype *iterator = NULL; iterator == NULL; iterator = (itype *) 1) \
	    for (link_t *_link = (list).head.next; \
	    iterator = list_get_instance(_link, itype, member), \
	    _link != &(list).head; _link = _link->next)

#define list_foreach_rev(list, member, itype, iterator) \
	for (itype *iterator = NULL; iterator == NULL; iterator = (itype *) 1) \
	    for (link_t *_link = (list).head.prev; \
	    iterator = list_get_instance(_link, itype, member), \
	    _link != &(list).head; _link = _link->prev)

extern void list_initialize(list_t *);
extern void link_initialize(link_t *);
extern void list_insert_before(link_t *, link_t *);
extern void list_insert_after(link_t *, link_t *);
extern void list_prepend(link_t *, list_t *);
extern void list_append(link_t *, list_t *);
extern void list_remove(link_t *);
extern bool link_used(link_t *);
extern bool list_empty(list_t *);
extern unsigned long list_count(list_t *);
extern link_t *list_first(list_t *);
extern link_t *list_last(list_t *);
extern link_t *list_prev(link_t *, list_t *);
extern link_t *list_next(link_t *, list_t *);

#endif
