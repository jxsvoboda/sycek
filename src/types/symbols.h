/*
 * Copyright 2022 Jiri Svoboda
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
 * Symbols
 */

#ifndef TYPES_SYMBOLS_H
#define TYPES_SYMBOLS_H

#include <adt/list.h>

/** Symbols */
typedef struct symbols {
	/** Scope members */
	list_t syms; /* of symbol_t */
} symbols_t;

/** Scope member type */
typedef enum {
	/** Variable */
	st_var,
	/** Function */
	st_fun,
	/** Typedef */
	st_type
} symbol_type_t;

/** Symbol flags */
typedef enum {
	/** Symbol is defined (note: a symbol, if it exists, is always declared) */
	sf_defined = 0x1,
	/** Symbol is used */
	st_used = 0x2
} symbol_flags_t;

/** Symbol */
typedef struct {
	/** Containing symbol index */
	symbols_t *symbols;
	/** Link to symbols_t.syms */
	link_t lsyms;
	/** Identifier */
	char *ident;
	/** Symbol type */
	symbol_type_t stype;
	/** Symbol flags */
	symbol_flags_t flags;
	/** Code generator type */
	struct cgtype *cgtype;
} symbol_t;

#endif
