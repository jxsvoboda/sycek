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
 * C preprocessor macro expander
 */

#ifndef TYPES_PREPROC_EXPANDER_H
#define TYPES_PREPROC_EXPANDER_H

#include <stddef.h>
#include <types/input_buf.h>
#include <types/linput.h>
#include <types/src_pos.h>

enum {
	preproc_expander_buf_size = 32
};

/** C preprocessor macro expander */
typedef struct preproc_expander {
	/** Preprocessor */
	struct preproc *preproc;
	/** Input buffer */
	input_buf_t *inbuf;
	/** Current position */
	src_pos_t pos;
	/** Output buffer */
	char out_buf[preproc_expander_buf_size];
	/** Output position buffer */
	src_pos_t out_posbuf[preproc_expander_buf_size];
	/** Number of used bytes in out_buf */
	size_t out_buf_used;
	/** Position of beginning of output buffer */
	src_pos_t out_buf_pos;
	/** Expander hit end of line. */
	bool expand_eol;
} preproc_expander_t;

#endif
