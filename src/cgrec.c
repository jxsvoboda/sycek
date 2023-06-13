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
 * Code generator record definitions
 */

#include <cgrec.h>
#include <cgtype.h>
#include <ir.h>
#include <merrno.h>
#include <stdlib.h>
#include <string.h>

static void cgen_rec_elem_destroy(cgen_rec_elem_t *);

/** Create code generator records list.
 *
 * @param rrecords Place to store pointer to new records list
 * @return EOK on success, ENOMEM if out of memory
 */
int cgen_records_create(cgen_records_t **rrecords)
{
	cgen_records_t *records;

	records = calloc(1, sizeof(cgen_records_t));
	if (records == NULL)
		return ENOMEM;

	list_initialize(&records->records);
	*rrecords = records;
	return EOK;
}

/** Destroy code generator records list.
 *
 * @param records Records list or @c NULL
 */
void cgen_records_destroy(cgen_records_t *records)
{
	cgen_record_t *record;

	if (records == NULL)
		return;

	record = cgen_records_first(records);
	while (record != NULL) {
		cgen_record_destroy(record);
		record = cgen_records_first(records);
	}

	free(records);
}

/** Create record definition.
 *
 * @param records Records list
 * @param rtype Record type (struct or union)
 * @param cident C identifier or @c NULL if anonymous
 * @param irident IR identifier
 * @param irrec IR record
 * @param rrecord Place to store pointer to new record definition
 * @return EOK on success or an error code
 */
int cgen_record_create(cgen_records_t *records, cgen_rec_type_t rtype,
    const char *cident, const char *irident, ir_record_t *irrec,
    cgen_record_t **rrecord)
{
	cgen_record_t *record;

	record = calloc(1, sizeof(cgen_record_t));
	if (record == NULL)
		return ENOMEM;

	record->rtype = rtype;

	if (cident != NULL) {
		record->cident = strdup(cident);
		if (record->cident == NULL)
			goto error;
	}

	record->irident = strdup(irident);
	if (record->irident == NULL)
		goto error;

	record->irrecord = irrec;

	list_initialize(&record->elems);

	record->records = records;
	list_append(&record->lrecords, &record->records->records);
	*rrecord = record;
	return EOK;
error:
	if (record->cident != NULL)
		free(record->cident);
	if (record->irident != NULL)
		free(record->irident);
	free(record);
	return ENOMEM;
}

/** Find a record definition.
 *
 * @param records Records list
 * @param ident Record identifier
 * @return Record or @c NULL if not found
 */
cgen_record_t *cgen_records_find(cgen_records_t *records, const char *ident)
{
	cgen_record_t *record;

	record = cgen_records_first(records);
	while (record != NULL) {
		if (record->cident != NULL &&
		    strcmp(record->cident, ident) == 0)
			return record;

		record = cgen_records_next(record);
	}

	return NULL;
}

/** Destroy record definition.
 *
 * @param record Record definition
 */
void cgen_record_destroy(cgen_record_t *record)
{
	cgen_rec_elem_t *elem;

	list_remove(&record->lrecords);

	elem = cgen_record_first(record);
	while (elem != NULL) {
		cgen_rec_elem_destroy(elem);
		elem = cgen_record_first(record);
	}

	if (record->cident != NULL)
		free(record->cident);
	if (record->irident != NULL)
		free(record->irident);
	free(record);
}

/** Get first record definition.
 *
 * @param record Records list
 * @return First record or @c NULL if there are none
 */
cgen_record_t *cgen_records_first(cgen_records_t *records)
{
	link_t *link;

	link = list_first(&records->records);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_record_t, lrecords);
}

/** Get next record definition.
 *
 * @param cur Current record definition
 * @return Next record definition or @c NULL if @a cur was the last one
 */
cgen_record_t *cgen_records_next(cgen_record_t *cur)
{
	link_t *link;

	link = list_next(&cur->lrecords, &cur->records->records);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_record_t, lrecords);
}

/** Append new element to record definition.
 *
 * @param record Record definition
 * @param ident Member identifier
 * @param cgtype Member CG type
 * @return EOK on success, EEXIST if member with the same name already
 *         exists, ENOMEM if out of memory.
 */
int cgen_record_append(cgen_record_t *record, const char *ident,
    cgtype_t *cgtype)
{
	cgen_rec_elem_t *elem;
	int rc;

	elem = cgen_record_elem_find(record, ident);
	if (elem != NULL)
		return EEXIST;

	elem = calloc(1, sizeof(cgen_rec_elem_t));
	if (elem == NULL)
		return ENOMEM;

	elem->record = record;
	elem->ident = strdup(ident);
	if (elem->ident == NULL) {
		free(elem);
		return ENOMEM;
	}

	rc = cgtype_clone(cgtype, &elem->cgtype);
	if (rc != EOK) {
		free(elem->ident);
		free(elem);
		return ENOMEM;
	}

	list_append(&elem->lelems, &record->elems);
	return EOK;
}

/** Look up record element by identifier.
 *
 * @param record Record definition
 * @param ident Element indentifier
 * @return Record element or @c NULL if not found
 */
cgen_rec_elem_t *cgen_record_elem_find(cgen_record_t *record,
    const char *ident)
{
	cgen_rec_elem_t *elem;

	elem = cgen_record_first(record);
	while (elem != NULL) {
		if (strcmp(elem->ident, ident) == 0)
			return elem;

		elem = cgen_record_next(elem);
	}

	return NULL;
}

/** Return first record element.
 *
 * @param rec Record definition
 * @return First record element or @c NULL there are none
 */
cgen_rec_elem_t *cgen_record_first(cgen_record_t *record)
{
	link_t *link;

	link = list_first(&record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lelems);
}

/** Return next record element.
 *
 * @param cur Current record element
 * @return Next record element or @c NULL if @cur was the last
 */
cgen_rec_elem_t *cgen_record_next(cgen_rec_elem_t *cur)
{
	link_t *link;

	link = list_next(&cur->lelems, &cur->record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lelems);
}

/** Destroy record element.
 *
 * @param elem Record element
 */
static void cgen_rec_elem_destroy(cgen_rec_elem_t *elem)
{
	list_remove(&elem->lelems);
	free(elem->ident);
	cgtype_destroy(elem->cgtype);
	free(elem);
}
