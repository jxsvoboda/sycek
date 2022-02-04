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
 * Identifier scope
 */

#ifndef TYPES_SCOPE_H
#define TYPES_SCOPE_H

#include <adt/list.h>

/** Scope */
typedef struct scope {
	/** Parent scope or @c NULL */
	struct scope *parent;
	/** Scope members */
	list_t members; /* of scope_member_t */
} scope_t;

/** Scope member type */
typedef enum {
	/** Global symbol */
	sm_gsym,
	/** Function argument */
	sm_arg,
	/** Local variable */
	sm_lvar
} scope_member_type_t;

/** Scope member - function argument */
typedef struct {
	/** Argument IR variable identifier (e.g. '%0', '%1', etc.) */
	char *vident;
} scope_member_arg_t;

/** Scope member - local variable */
typedef struct {
	/** IR variable identifier (e.g. '%foo') */
	char *vident;
} scope_member_lvar_t;

/** Scope member */
typedef struct {
	/** Containing scope */
	scope_t *scope;
	/** Link to scope_t.members */
	link_t lmembers;
	/** Identifier */
	char *ident;
	/** Member type */
	scope_member_type_t mtype;
	union {
		scope_member_arg_t arg;
		scope_member_lvar_t lvar;
	} m;
} scope_member_t;

#endif
