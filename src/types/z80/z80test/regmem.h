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
 * Register/memory script operand
 */

#ifndef TYPES_Z80TEST_REGMEM_H
#define TYPES_Z80TEST_REGMEM_H

#include <stdint.h>

/** Register/memory type */
typedef enum {
	/** AF register pair */
	rm_AF,
	/** BC register pair */
	rm_BC,
	/** DE register pair */
	rm_DE,
	/** HL register pair */
	rm_HL,
	/** A register */
	rm_A,
	/** B register */
	rm_B,
	/** C register */
	rm_C,
	/** D register */
	rm_D,
	/** E register */
	rm_E,
	/** H register */
	rm_H,
	/** L register */
	rm_L,
	/** Byte pointer */
	rm_byte_ptr,
	/** Word pointer */
	rm_word_ptr,
	/** Doubleword pointer */
	rm_dword_ptr,
	/** Quadword pointer */
	rm_qword_ptr
} regmem_type_t;

/** Source code position */
typedef struct {
	/** Register/memory operand type */
	regmem_type_t rmtype;
	/** Memory address, if applicable */
	uint16_t addr;
} regmem_t;

#endif
