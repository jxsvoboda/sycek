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
 * Standard ROM tape block format
 */

#ifndef TYPES_TAPE_ROMBLOCK_H
#define TYPES_TAPE_ROMBLOCK_H

#include <stdint.h>

enum {
	/** Standard file header */
	bflag_header = 0x00,
	/** Standard file data */
	bflag_data = 0xff
};

typedef enum {
	/** Program */
	ftype_program = 0x00,
	/** Number array */
	ftype_number_array = 0x01,
	/** Character array */
	ftype_character_array = 0x02,
	/** Bytes */
	ftype_bytes = 0x03
} rom_ftype_t;

/** Standard ROM header block (19 bytes) */
typedef struct {
	/** 0x00 for standard header */
	uint8_t flag;
	/** File type */
	uint8_t ftype;
	/** File name */
	uint8_t fname[10];
	/** Length of data block */
	uint16_t dblen;
	/** Parameter 1 */
	uint16_t param1;
	/** Parameter 2 */
	uint16_t param2;
	/** Parity byte */
	uint8_t parity;
} __attribute__((packed)) rom_tape_header_t;

/** Structure for holding Spectrum tape file name */
typedef struct {
	/** Buffer to hold file name and null character */
	char fname[11];
} rom_filename_t;

#endif
