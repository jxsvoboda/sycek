/*
 * Copyright 2025 Jiri Svoboda
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

#ifndef TYPES_CGREC_H
#define TYPES_CGREC_H

#include <adt/list.h>
#include <stdbool.h>

/** Record type */
typedef enum {
	/** Struct */
	cgr_struct,
	/** Union */
	cgr_union
} cgen_rec_type_t;

/** Record element */
typedef struct cgen_rec_elem {
	/** Containing record definition */
	struct cgen_record *record;
	/** Link to @c record->elems */
	link_t lelems;
	/** Member identifier */
	char *ident;
	/** Member type */
	struct cgtype *cgtype;
} cgen_rec_elem_t;

/** Record definition */
typedef struct cgen_record {
	/** Containing records structure */
	struct cgen_records *records;
	/** Link to @c records->records */
	link_t lrecords;
	/** Record type */
	cgen_rec_type_t rtype;
	/** C identifier */
	char *cident;
	/** IR identifier */
	char *irident;
	/** IR record */
	struct ir_record *irrecord;
	/** @c true iff record is in process of being defined */
	bool defining;
	/** Record elements (of cgen_rec_elem_t) */
	list_t elems;
} cgen_record_t;

/** Record definitions */
typedef struct cgen_records {
	/** Record definitions (of cgen_record_t) */
	list_t records;
} cgen_records_t;

#endif
