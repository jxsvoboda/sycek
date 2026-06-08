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
 * Tape image
 */

#ifndef TYPES_TAPE_TAPE_H
#define TYPES_TAPE_TAPE_H

#include <adt/list.h>
#include <stdint.h>

/** Tape block */
typedef struct tape_block {
	/** Containing tape */
	struct tape *tape;
	/** Link to @c tape->blocks */
	link_t lblocks;
	/** Block data */
	uint8_t *data;
	/** Block size */
	uint16_t size;
	/** Pause after block in ms */
	uint16_t pause_after;
} tape_block_t;

/** Tape image */
typedef struct tape {
	/** Major version */
	uint8_t major;
	/** Minor version */
	uint8_t minor;
	/** Blocks */
	list_t blocks; /* of tape_block_t */
} tape_t;

#endif
