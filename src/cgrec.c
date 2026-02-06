/*
 * Copyright 2026 Jiri Svoboda
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
static void cgen_rec_stor_destroy(cgen_rec_stor_t *);

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
	list_initialize(&record->stors);

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
	cgen_rec_stor_t *stor;

	list_remove(&record->lrecords);

	elem = cgen_record_first_elem(record);
	while (elem != NULL) {
		cgen_rec_elem_destroy(elem);
		elem = cgen_record_first_elem(record);
	}

	stor = cgen_record_first_stor(record);
	while (stor != NULL) {
		cgen_rec_stor_destroy(stor);
		stor = cgen_record_first_stor(record);
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

/** Append new element+storage unit to record definition.
 *
 * If width is not zero, only element is appended, but no storage unit.
 *
 * @param record Record definition
 * @param ident Member identifier
 * @param width Bit width (bit field) or 0 (plain field)
 * @param bitpos Bit position in storage unit or zero (plain field)
 * @param cgtype Member CG type
 * @param irident IR identifier
 * @return EOK on success, EEXIST if member with the same name already
 *         exists, ENOMEM if out of memory.
 */
int cgen_record_append(cgen_record_t *record, const char *ident,
    unsigned width, unsigned bitpos, cgtype_t *cgtype, const char *irident)
{
	cgen_rec_elem_t *old_elem;
	cgen_rec_elem_t *elem = NULL;
	cgen_rec_stor_t *stor = NULL;
	int rc;

	old_elem = cgen_record_elem_find(record, ident, NULL);
	if (old_elem != NULL) {
		rc = EEXIST;
		goto error;
	}

	elem = calloc(1, sizeof(cgen_rec_elem_t));
	if (elem == NULL) {
		rc = ENOMEM;
		goto error;
	}

	/*
	 * For a plain element we immediately create a storage unit, because
	 * they are mapped 1:1. We do not create a storage unit for a bit
	 * field, because the storage unit (common for one or more elements)
	 * will be created separately.
	 */
	if (width == 0) {
		stor = calloc(1, sizeof(cgen_rec_stor_t));
		if (stor == NULL) {
			rc = ENOMEM;
			goto error;
		}
	}

	elem->record = record;
	elem->ident = strdup(ident);
	if (elem->ident == NULL) {
		rc = ENOMEM;
		goto error;
	}

	elem->width = width;
	elem->bitpos = bitpos;

	rc = cgtype_clone(cgtype, &elem->cgtype);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	elem->stor = stor;

	if (stor != NULL) {
		stor->record = record;
		stor->irident = strdup(irident);
		if (stor->irident == NULL) {
			rc = ENOMEM;
			goto error;
		}

		rc = cgtype_clone(cgtype, &stor->cgtype);
		if (rc != EOK) {
			rc = ENOMEM;
			goto error;
		}

		list_append(&stor->lstors, &record->stors);
	}

	list_append(&elem->lrec_elems, &record->elems);
	return EOK;
error:
	if (elem != NULL) {
		if (elem->cgtype != NULL)
			cgtype_destroy(elem->cgtype);
		if (elem->ident != NULL)
			free(elem->ident);
		free(elem);
	}
	if (stor != NULL) {
		if (stor->irident != NULL)
			free(stor->irident);
		free(stor);
	}

	return rc;
}

/** Append new storage unit to record definition (for hodling bit fields).
 *
 * @param record Record definition
 * @param cgtype Storage unit CG type
 * @param irident IR identifier
 * @param rstor Place to store pointer to new storage unit or @c NULL
 * @return EOK on success, EEXIST if member with the same name already
 *         exists, ENOMEM if out of memory.
 */
int cgen_record_append_stor(cgen_record_t *record, cgtype_t *cgtype,
    const char *irident, cgen_rec_stor_t **rstor)
{
	cgen_rec_stor_t *stor = NULL;
	int rc;

	stor = calloc(1, sizeof(cgen_rec_stor_t));
	if (stor == NULL) {
		rc = ENOMEM;
		goto error;
	}

	list_initialize(&stor->elems);
	stor->record = record;
	stor->bitfield = true;
	stor->irident = strdup(irident);
	if (stor->irident == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = cgtype_clone(cgtype, &stor->cgtype);
	if (rc != EOK) {
		rc = ENOMEM;
		goto error;
	}

	list_append(&stor->lstors, &record->stors);
	if (rstor != NULL)
		*rstor = stor;
	return EOK;
error:
	if (stor != NULL) {
		if (stor->irident != NULL)
			free(stor->irident);
		free(stor);
	}

	return rc;
}

/** Look up record element by identifier.
 *
 * @param record Record definition
 * @param ident Element identifier
 * @param ridx Place to store element index or @c NULL if not interested
 * @return Record element or @c NULL if not found
 */
cgen_rec_elem_t *cgen_record_elem_find(cgen_record_t *record,
    const char *ident, uint64_t *ridx)
{
	cgen_rec_elem_t *elem;
	uint64_t idx;

	elem = cgen_record_first_elem(record);
	idx = 0;
	while (elem != NULL) {
		if (strcmp(elem->ident, ident) == 0) {
			if (ridx != NULL)
				*ridx = idx;
			return elem;
		}

		++idx;
		elem = cgen_record_next_elem(elem);
	}

	return NULL;
}

/** Return first record element.
 *
 * @param rec Record definition
 * @return First record element or @c NULL there are none
 */
cgen_rec_elem_t *cgen_record_first_elem(cgen_record_t *record)
{
	link_t *link;

	link = list_first(&record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lrec_elems);
}

/** Return next record element.
 *
 * @param cur Current record element
 * @return Next record element or @c NULL if @cur was the last
 */
cgen_rec_elem_t *cgen_record_next_elem(cgen_rec_elem_t *cur)
{
	link_t *link;

	link = list_next(&cur->lrec_elems, &cur->record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lrec_elems);
}

/** Return last record element.
 *
 * @param rec Record definition
 * @return Last record element or @c NULL there are none
 */
cgen_rec_elem_t *cgen_record_last_elem(cgen_record_t *record)
{
	link_t *link;

	link = list_last(&record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lrec_elems);
}

/** Return previous record element.
 *
 * @param cur Current record element
 * @return Previous record element or @c NULL if @cur was the first
 */
cgen_rec_elem_t *cgen_record_prev_elem(cgen_rec_elem_t *cur)
{
	link_t *link;

	link = list_prev(&cur->lrec_elems, &cur->record->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lrec_elems);
}

/** Destroy record element.
 *
 * @param elem Record element
 */
static void cgen_rec_elem_destroy(cgen_rec_elem_t *elem)
{
	list_remove(&elem->lrec_elems);
	free(elem->ident);
	cgtype_destroy(elem->cgtype);
	free(elem);
}

/** Return first record storage unit.
 *
 * @param rec Record definition
 * @return First record storage unit or @c NULL there are none
 */
cgen_rec_stor_t *cgen_record_first_stor(cgen_record_t *record)
{
	link_t *link;

	link = list_first(&record->stors);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_stor_t, lstors);
}

/** Return next record storage unit.
 *
 * @param cur Current record storage unit
 * @return Next record storage unit or @c NULL if @cur was the last
 */
cgen_rec_stor_t *cgen_record_next_stor(cgen_rec_stor_t *cur)
{
	link_t *link;

	link = list_next(&cur->lstors, &cur->record->stors);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_stor_t, lstors);
}

/** Destroy record storage unit.
 *
 * @param stor Record storage unit
 */
static void cgen_rec_stor_destroy(cgen_rec_stor_t *stor)
{
	list_remove(&stor->lstors);
	free(stor->irident);
	cgtype_destroy(stor->cgtype);
	free(stor);
}

/** Return first record storage unit element.
 *
 * @param stor Storage unit
 * @return First storage unit element or @c NULL there are none
 */
cgen_rec_elem_t *cgen_rec_stor_first_elem(cgen_rec_stor_t *stor)
{
	link_t *link;

	link = list_first(&stor->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lstor_elems);
}

/** Return next record storage unit element.
 *
 * @param cur Current record element
 * @return Next record element or @c NULL if @cur was the last
 */
cgen_rec_elem_t *cgen_rec_stor_next_elem(cgen_rec_elem_t *cur)
{
	link_t *link;

	link = list_next(&cur->lstor_elems, &cur->stor->elems);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, cgen_rec_elem_t, lstor_elems);
}
