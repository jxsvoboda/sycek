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
 * Code generator C types
 */

#ifndef TYPES_CGTYPE_H
#define TYPES_CGTYPE_H

/** Code generator type node type */
typedef enum {
	/** Basic type */
	cgn_basic,
	/** Pointer type */
	cgn_pointer
} cgtype_ntype_t;

/** Elementary types */
typedef enum {
	cgelm_void,
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
	cgelm_logic
} cgtype_elmtype_t;

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
	/* TODO signed / unsigned */
} cgtype_basic_t;

/** Pointer type */
typedef struct {
	/** Base type object */
	cgtype_t cgtype;
	/** Type of the pointer target */
	cgtype_t *tgtype;
} cgtype_pointer_t;

#endif
