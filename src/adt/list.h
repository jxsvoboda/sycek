/*
 * Copyright 2018 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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
