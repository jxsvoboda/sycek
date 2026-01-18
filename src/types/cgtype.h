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
 * Code generator C types
 */

#ifndef TYPES_CGTYPE_H
#define TYPES_CGTYPE_H

#include <adt/list.h>
#include <stdint.h>

/** Code generator type node type */
typedef enum {
	/** Basic type */
	cgn_basic,
	/** Function type */
	cgn_func,
	/** Pointer type */
	cgn_pointer,
	/** Record type */
	cgn_record,
	/** Enum type */
	cgn_enum,
	/** Array type */
	cgn_array
} cgtype_ntype_t;

/** Elementary types */
typedef enum {
	cgelm_void,
	cgelm_bool,
	/* TODO distinguish between char and signed/unsigned char? */
	cgelm_char,
	cgelm_uchar,
	cgelm_short,
	cgelm_ushort,
	cgelm_int,
	cgelm_uint,
	cgelm_long,
	cgelm_ulong,
	cgelm_longlong,
	cgelm_ulonglong,
	/* TODO float, double, long double */
	/* TODO _Bool */
	cgelm_logic,
	cgelm_va_list
} cgtype_elmtype_t;

/** Integer type rank.
 *
 * This is useful for determining the result type of Usual Arithmetic
 * Conversion and is defined by the standard
 */
typedef enum {
	/** Char is the lowest rank */
	cgir_char,
	/** Short */
	cgir_short,
	/** Int */
	cgir_int,
	/** Long */
	cgir_long,
	/** Long long is the highest rank */
	cgir_longlong
} cgtype_int_rank_t;

/** Calling convention */
typedef enum {
	/** Default calling convention */
	cgcc_default,
	/** User service routine */
	cgcc_usr
} cgtype_cconv_t;

/** Code generator C type */
typedef struct cgtype {
	/** Entire / type-specific structure */
	void *ext;
	/** Node type */
	cgtype_ntype_t ntype;
} cgtype_t;

/** Basic type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Elementary type */
	cgtype_elmtype_t elmtype;
} cgtype_basic_t;

/** Function type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Return type */
	cgtype_t *rtype;
	/** Arguments (of cgtype_func_arg_t) */
	list_t args;
	/** Variadic? */
	bool variadic;
	/** Calling convention */
	cgtype_cconv_t cconv;
} cgtype_func_t;

/** Function argument type */
typedef struct {
	/** Containing function type */
	cgtype_func_t *func;
	/** Link to @c func->args */
	link_t largs;
	/** Argument type */
	cgtype_t *atype;
} cgtype_func_arg_t;

/** Pointer type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Type of the pointer target */
	cgtype_t *tgtype;
} cgtype_pointer_t;

/** Record type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Code generator record definition */
	struct cgen_record *record;
} cgtype_record_t;

/** Enum type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Code generator enum definition */
	struct cgen_enum *cgenum;
} cgtype_enum_t;

/** Array type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Array element type */
	cgtype_t *etype;
	/** Array index type or @c NULL if not known */
	cgtype_t *itype;
	/** @c true iff array has a specified size */
	bool have_size;
	/** Array size (only valid if @c have_size is @c true) */
	uint64_t asize;
} cgtype_array_t;

#endif
