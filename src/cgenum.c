/*
 * Copyright 2023 Jiri Svoboda
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
 * Code generator enum definitions
 */

#include <cgenum.h>
#include <cgtype.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>

static void cgen_enum_elem_destroy(cgen_enum_elem_t *);

/** Create code generator enums list.
 *
 * @param renums Place to store pointer to new enums list
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_enums_create(cgen_enums_t **renums)
{
	cgen_enums_t *enums;

	enums = calloc(1, sizeof(cgen_enums_t));
	if (enums == NULL)
		return ENOMEM;

	list_initialize(&enums->enums);
	*renums = enums;
	return EOK;
}

/** Destroy code generator enums list.
 *
 * @param enums Enums list or @c NULL
 */
void cgen_enums_destroy(cgen_enums_t *enums)
{
	cgen_enum_t *cgenum;

	if (enums == NULL)
		return;

	cgenum = cgen_enums_first(enums);
	while (cgenum != NULL) {
		cgen_enum_destroy(cgenum);
		cgenum = cgen_enums_first(enums);
	}

	free(enums);
}

/** Create enum definition.
 *
 * @param records Records list
 * @param rtype Record type (struct or union)
 * @param cident C identifier or @c NULL if anonymous
 * @param irident IR identifier
 * @param irrec IR record
 * @param rrecord Place to store pointer to new record definition
 * @return EOK on success or an error code
 */
int cgen_enum_create(cgen_enums_t *enums, const char *cident,
    cgen_enum_t **renum)
{
	cgen_enum_t *cgenum;

	cgenum = calloc(1, sizeof(cgen_enum_t));
	if (cgenum == NULL)
		return ENOMEM;

	if (cident != NULL) {
		cgenum->cident = strdup(cident);
		if (cgenum->cident == NULL)
			goto error;
	}

	list_initialize(&cgenum->elems);

	cgenum->enums = enums;
	list_append(&cgenum->lenums, &cgenum->enums->enums);
	*renum = cgenum;
	return EOK;
error:
	if (cgenum->cident != NULL)
		free(cgenum->cident);
	free(cgenum);
	return ENOMEM;
}

/** Find an enum definition.
 *
 * @param enums Enums list
 * @param ident Enum identifier
 * @return Enum or @c NULL if not found
 */
cgen_enum_t *cgen_enums_find(cgen_enums_t *enums, const char *ident)
{
	cgen_enum_t *cgenum;

	cgenum = cgen_enums_first(enums);
	while (cgenum != NULL) {
		if (cgenum->cident != NULL &&
		    strcmp(cgenum->cident, ident) == 0)
			return cgenum;

		cgenum = cgen_enums_next(cgenum);
	}

	return NULL;
}

/** Destroy enum definition.
 *
 * @param cgenum Enum definition
 */
void cgen_enum_destroy(cgen_enum_t *cgenum)
{
	cgen_enum_elem_t *elem;

	list_remove(&cgenum->lenums);

	elem = cgen_enum_first(cgenum);
	while (elem != NULL) {
		cgen_enum_elem_destroy(elem);
		elem = cgen_enum_first(cgenum);
	}

	if (cgenum->cident != NULL)
		free(cgenum->cident);
	free(cgenum);
}

/** Get first enum definition.
 *
 * @param enums Enums list
 * @return First enum or @c NULL if there are none
 */
cgen_enum_t *cgen_enums_first(cgen_enums_t *enums)
{
	link_t *link;

	link = list_first(&enums->enums);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_enum_t, lenums);
}

/** Get next enum definition.
 *
 * @param cur Current enum definition
 * @return Next enum definition or @c NULL if @a cur was the last one
 */
cgen_enum_t *cgen_enums_next(cgen_enum_t *cur)
{
	link_t *link;

	link = list_next(&cur->lenums, &cur->enums->enums);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_enum_t, lenums);
}

/** Append new element to enum definition.
 *
 * @param cgenum Enum definition
 * @param ident Member identifier
 * @param value Member value
 * @param cgtype Member CG type
 * @param relem Place to store pointer to new element or @c NULL
 *              if not interested
 * @return EOK on success, EEXIST if member with the same name already
 *         exists, ENOMEM if out of memory.
 */
int cgen_enum_append(cgen_enum_t *cgenum, const char *ident, int value,
    cgen_enum_elem_t **relem)
{
	cgen_enum_elem_t *elem;

	elem = cgen_enum_elem_find(cgenum, ident);
	if (elem != NULL)
		return EEXIST;

	elem = calloc(1, sizeof(cgen_enum_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->cgenum = cgenum;
	elem->ident = strdup(ident);
	if (elem->ident == NULL) {
		free(elem);
		return ENOMEM;
	}

	elem->value = value;

	list_append(&elem->lelems, &cgenum->elems);
	if (relem != NULL)
		*relem = elem;
	return EOK;
}

/** Look up enum element by identifier.
 *
 * @param cgenum Enum definition
 * @param ident Element indentifier
 * @return Enum element or @c NULL if not found
 */
cgen_enum_elem_t *cgen_enum_elem_find(cgen_enum_t *cgenum,
    const char *ident)
{
	cgen_enum_elem_t *elem;

	elem = cgen_enum_first(cgenum);
	while (elem != NULL) {
		if (strcmp(elem->ident, ident) == 0)
			return elem;

		elem = cgen_enum_next(elem);
	}

	return NULL;
}

/** Look up enum element by value.
 *
 * @param cgenum Enum definition
 * @param val Value
 * @return Enum element or @c NULL if not found
 */
cgen_enum_elem_t *cgen_enum_val_find(cgen_enum_t *cgenum, int val)
{
	cgen_enum_elem_t *elem;

	elem = cgen_enum_first(cgenum);
	while (elem != NULL) {
		if (elem->value == val)
			return elem;

		elem = cgen_enum_next(elem);
	}

	return NULL;
}

/** Return first enum element.
 *
 * @param cgenum Enum definition
 * @return First enum element or @c NULL there are none
 */
cgen_enum_elem_t *cgen_enum_first(cgen_enum_t *cgenum)
{
	link_t *link;

	link = list_first(&cgenum->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_enum_elem_t, lelems);
}

/** Return next enum element.
 *
 * @param cur Current enum element
 * @return Next enum element or @c NULL if @cur was the last
 */
cgen_enum_elem_t *cgen_enum_next(cgen_enum_elem_t *cur)
{
	link_t *link;

	link = list_next(&cur->lelems, &cur->cgenum->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_enum_elem_t, lelems);
}

/** Destroy enum element.
 *
 * @param elem Enum element
 */
static void cgen_enum_elem_destroy(cgen_enum_elem_t *elem)
{
	list_remove(&elem->lelems);
	free(elem->ident);
	free(elem);
}
