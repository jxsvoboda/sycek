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
 * C preprocessor
 */

#ifndef TYPES_PREPROC_H
#define TYPES_PREPROC_H

#include <adt/list.h>
#include <types/linput.h>
#include <types/preproc.h>

enum {
	preproc_buf_size = 32,
	preproc_buf_low_watermark = 16,
	preproc_out_buf_size = 32
};

/** C preprocessor */
typedef struct preproc {
	/** Input ops */
	lexer_input_ops_t *input_ops;
	/** Argument to input ops */
	void *input_arg;
	/** State stack (list of preproc_state_entry_t */
	list_t states;
	/** Input buffer */
	char buf[preproc_buf_size];
	/** Input position buffer */
	src_pos_t posbuf[lexer_buf_size];
	/** Buffer position */
	size_t buf_pos;
	/** Number of used bytes in buf */
	size_t buf_used;
	/** Current position */
	src_pos_t pos;
	/** EOF hit in input */
	bool in_eof;
	/** Error hit in input */
	bool in_error;
	/** Position of beginning of input buffer */
	src_pos_t buf_bpos;
	/** Output buffer */
	char out_buf[preproc_out_buf_size];
	/** Number of used bytes in out_buf */
	size_t out_buf_used;
	/** Position of beginning of output buffer */
	src_pos_t out_buf_pos;
	int pluslim;
} preproc_t;

/** C preprocessor state */
typedef enum {
	/** At beginning of line (or after whitespace) */
	pps_line_begin,
	/** Inside a line of text */
	pps_text_line
} preproc_state_t;

/** C preprocessor state stack entry */
typedef struct {
	/** Containing preprocessor */
	struct preproc *preproc;
	/** Link to @c preproc->states */
	link_t lstates;
	/** State */
	preproc_state_t state;
} preproc_state_entry_t;

#endif
