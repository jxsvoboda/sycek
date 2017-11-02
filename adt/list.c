/*
 * Linked list
 */

#include <adt/list.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/** Initialize list.
 *
 * @param list List
 */
void list_initialize(list_t *list)
{
	list->head.prev = &list->head;
	list->head.next = &list->head;
}

/** Initialize link.
 *
 * @param link Link
 */
void link_initialize(link_t *link)
{
	link->prev = NULL;
	link->next = NULL;
}

/** Insert item before item in list.
 *
 * @param nlink New item
 * @param olink Existing item
 */
void list_insert_before(link_t *nlink, link_t *olink)
{
	assert(!link_used(nlink));

	olink->prev->next = nlink;
	nlink->prev = olink->prev;
	nlink->next = olink;
	olink->prev = nlink;
}

/** Insert item after item in list.
 *
 * @param nlink New item
 * @param olink Existing item
 */
void list_insert_after(link_t *nlink, link_t *olink)
{
	assert(!link_used(nlink));

	olink->next->prev = nlink;
	nlink->next = olink->next;
	nlink->prev = olink;
	olink->next = nlink;
}

/** Insert at beginning of list.
 *
 * @param link New item
 * @param list List
 */
void list_prepend(link_t *link, list_t *list)
{
	list_insert_after(link, &list->head);
}

/** Insert at end of list.
 *
 * @param link New item
 * @param list List
 */
void list_append(link_t *link, list_t *list)
{
	list_insert_before(link, &list->head);
}

/** Remove item from list.
 *
 * @param link Item to remove
 */
void list_remove(link_t *link)
{
	assert(link_used(link));

	link->prev->next = link->next;
	link->next->prev = link->prev;

	link->prev = NULL;
	link->next = NULL;
}

/** Return true if item is linked to a list.
 *
 * @param link Item
 * @return @c true if @a link is linked to a list, @c false otherwise
 */
bool link_used(link_t *link)
{
	if (link->prev == NULL && link->next == NULL)
		return false;

	assert(link->prev != NULL && link->next != NULL);
	return true;
}

/** Return true if list is empty.
 *
 * @param list List
 * @return @c true if @a list is empty, @c false otherwise
 */
bool list_empty(list_t *list)
{
	return list->head.next == &list->head;
}

/** Return the number of entries in @a list.
 *
 * @param list List
 * @return Number of items in list
 */
unsigned long list_count(list_t *list)
{
	link_t *link;
	unsigned long count;

	count = 0;
	link = list_first(list);
	while (link != NULL) {
		++count;
		link = list_next(link, list);
	}

	return count;
}

/** Return first item in a list or @c NULL if list is empty.
 *
 * @param list List
 * @return First item or @c NULL if list is empty
 */
link_t *list_first(list_t *list)
{
	if (list->head.next == &list->head)
		return NULL;

	return list->head.next;
}

/** Return last item in a list or @c NULL if list is empty.
 *
 * @param list List
 * @return Last item or @c NULL if list is empty
 */
link_t *list_last(list_t *list)
{
	if (list->head.prev == &list->head)
		return NULL;

	return list->head.prev;
}

/** Return previous item in list or @c NULL if @a link is the first one.
 *
 * @param link Current item
 * @param list List to which @a link belongs
 *
 * @return Previous item or @c NULL if @a link is the first item in the list
 */
link_t *list_prev(link_t *link, list_t *list)
{
	assert(link_used(link));

	if (link->prev == &list->head)
		return NULL;

	return link->prev;
}

/** Return next item in list or @c NULL if @a link is the last one.
 *
 * @param link Current item
 * @param list List to which @a link belongs
 *
 * @return Next item or @c NULL if @a link is the last item in the list
 */
link_t *list_next(link_t *link, list_t *list)
{
	assert(link_used(link));

	if (link->next == &list->head)
		return NULL;

	return link->next;
}
