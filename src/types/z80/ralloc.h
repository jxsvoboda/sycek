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
 * Z80 register allocation
 */

#ifndef TYPES_Z80_RALLOC_H
#define TYPES_Z80_RALLOC_H

#include <stdint.h>
#include <types/z80/z80ic.h>

/** Z80 register allocator */
typedef struct {
	int dummy;
} z80_ralloc_t;

/** Z80 register allocator for procedure */
typedef struct {
	/** Containing register allocator */
	z80_ralloc_t *ralloc;
	/** Procedure with VRs */
	z80ic_proc_t *vrproc;
	/** Next label number to allocate */
	unsigned next_label;
} z80_ralloc_proc_t;

/** Z80 data access using index register
 *
 * If the register allocator wants to access a location on the stack
 * (e.g. stack frame entry / spilled virtual register, or an
 * argument stored on the stack), we may need to set up an index register,
 * then emit a specific instruction using HL, IX or IY, then possibly
 * restore any modified registers.
 *
 * This structure tracks the index register setup.
 */
typedef struct {
	/** Displacement */
	int8_t disp;
} z80_idxacc_t;

#endif
