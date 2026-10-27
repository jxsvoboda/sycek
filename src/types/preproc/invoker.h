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
 * C preprocessor macro invoker
 */

#ifndef TYPES_PREPROC_INVOKER_H
#define TYPES_PREPROC_INVOKER_H

#include <stddef.h>
#include <types/src_pos.h>

enum {
	preproc_invoker_buf_size = 32
};

/** C preprocessor macro invoker */
typedef struct preproc_invoker {
	/** Preprocessor */
	struct preproc *preproc;
	/** Macro being invoked */
	preproc_macro_t *macro;
	/** Offset in the macro */
	size_t macro_off;
	/** Current position */
	src_pos_t pos;
	/** Output buffer */
	char out_buf[preproc_invoker_buf_size];
	/** Output position buffer */
	src_pos_t out_posbuf[preproc_invoker_buf_size];
	/** Number of used bytes in out_buf */
	size_t out_buf_used;
	/** Position of beginning of output buffer */
	src_pos_t out_buf_pos;
} preproc_invoker_t;

#endif
