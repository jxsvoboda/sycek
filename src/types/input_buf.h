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
 * Input buffer
 */

#ifndef TYPES_INPUT_BUF_H
#define TYPES_INPUT_BUF_H

#include <stdbool.h>
#include <stddef.h>
#include <types/linput.h>
#include <types/src_pos.h>

enum {
	input_buf_size = 32,
	input_buf_low_watermark = 16,
};

/** Input buffer */
typedef struct input_buf {
	/** Input ops */
	lexer_input_ops_t *input_ops;
	/** Argument to input ops */
	void *input_arg;
	/** EOF hit in input */
	bool in_eof;
	/** Error hit in input */
	bool in_error;
	/** Input buffer */
	char buf[input_buf_size];
	/** Input position buffer */
	src_pos_t posbuf[input_buf_size];
	/** Input buffer position */
	size_t buf_pos;
	/** Number of used bytes in buf */
	size_t buf_used;
	/** Current position */
	src_pos_t pos;
} input_buf_t;

#endif
