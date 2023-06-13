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

#ifndef TYPES_CGENUM_H
#define TYPES_CGENUM_H

#include <adt/list.h>
#include <stdbool.h>

/** Enum element */
typedef struct cgen_enum_elem {
	/** Containing enum definition */
	struct cgen_enum *cgenum;
	/** Link to @c cgenum->elems */
	link_t lelems;
	/** Member identifier */
	char *ident;
	/** Member value */
	int value;
} cgen_enum_elem_t;

/** Enum definition */
typedef struct cgen_enum {
	/** Containing enums structure */
	struct cgen_enums *enums;
	/** Link to @c enums->enums */
	link_t lenums;
	/** C identifier */
	char *cident;
	/** @c true iff enum is in process of being defined */
	bool defining;
	/** Enum elements (of cgen_enum_elem_t) */
	list_t elems;
	/** @c true iff enum is defined */
	bool defined;
} cgen_enum_t;

/** Enum definitions */
typedef struct cgen_enums {
	/** Enum definitions (of cgen_enum_t) */
	list_t enums;
} cgen_enums_t;

#endif
