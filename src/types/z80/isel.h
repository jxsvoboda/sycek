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

#ifndef TYPES_Z80_ISEL_H
#define TYPES_Z80_ISEL_H

#include <types/z80/vainfo.h>
#include <types/z80/varmap.h>

/** Z80 instruction selector */
typedef struct {
	/** IR module */
	struct ir_module *irmodule;
} z80_isel_t;

/** Z80 instruction selector for procedure */
typedef struct {
	/** Containing instruction selector */
	z80_isel_t *isel;
	/** Procedure identifier */
	char *ident;
	/** Variable - virtual register map */
	z80_varmap_t *varmap;
	/** Next label number to allocate */
	unsigned next_label;
	/** This procedure is a user service routine */
	bool usr;
	/** Variable argument info */
	z80_vainfo_t vainfo;
	/** Source IR procedure */
	struct ir_proc *irproc;
	/** Destination IC procedure */
	struct z80ic_proc *icproc;
} z80_isel_proc_t;

#endif
