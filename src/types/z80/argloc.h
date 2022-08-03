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
 * Z80 function argument locations
 */

#ifndef TYPES_Z80_ARGLOC_H
#define TYPES_Z80_ARGLOC_H

#include <adt/list.h>
#include <stdbool.h>
#include <types/z80/z80ic.h>

enum {
	z80_max_reg_entries = 7
};

/** Z80 function argument locations */
typedef struct {
	/** Entries (of z80_argloc_entry_t) */
	list_t entries;
	/** Number of bytes used on the stack */
	unsigned stack_used;
	/** Bit mask of used upper halves of 16-bit registers */
	bool r16h_used[z80ic_r16_limit];
	/** Bit mask of used lower halves of 16-bit registers */
	bool r16l_used[z80ic_r16_limit];
} z80_argloc_t;

/** Register part holding argument (upper, lower, entire 16-bit register) */
typedef enum {
	/** Upper half of 16-bit register */
	z80_argloc_h,
	/** Lower half of 16-bit register */
	z80_argloc_l,
	/** Entire 16-bit register */
	z80_argloc_hl
} z80_argloc_rp_t;

/** Entry mapping part of argument to register */
typedef struct {
	/** 16-bit register name */
	z80ic_r16_t reg;
	/** Register part used (upper, lower, entire) */
	z80_argloc_rp_t part;
} z80_argloc_reg_t;

/** Z80 function argument locations entry */
typedef struct {
	/** Containing argument locations */
	z80_argloc_t *argloc;
	/** Link to @c argloc->entries */
	link_t lentries;
	/** Argument identifier */
	char *ident;
	/** Number of register entries used */
	unsigned reg_entries;
	/** Register entries */
	z80_argloc_reg_t reg[z80_max_reg_entries];
	/** Stack offset (within stack argument area) */
	unsigned stack_off;
	/** Number of bytes occupied on the stack */
	unsigned stack_sz;
} z80_argloc_entry_t;

#endif
