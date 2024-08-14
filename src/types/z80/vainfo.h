/*
 * Copyright 2024 Jiri Svoboda
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
 * Z80 instruction selection
 */

#ifndef TYPES_Z80_VAINFO_H
#define TYPES_Z80_VAINFO_H

#include <stdint.h>
#include <types/z80/stackframe.h>

/** Z80 information needed for _va_start */
typedef struct {
	/** Current argument address relative to SFE or SFB */
	int16_t cur_off;
	/** Current argument address is relative to SFE or SFB? */
	z80sf_rel_t cur_rel;
	/** Remaining bytes in register image */
	uint16_t rem_bytes;
} z80_vainfo_t;

/** Z80 __va_list */
typedef struct {
	/** Pointer to current argument */
	uint16_t cur;
	/** Number of bytes remaining in register part or zero */
	uint16_t remain;
	/** Pointer to first stack argument */
	uint16_t stack;
} z80_va_list_t;

#endif
