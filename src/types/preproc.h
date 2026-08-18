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
#include <stdio.h>
#include <types/file_input.h>
#include <types/linput.h>
#include <types/preproc.h>
#include <types/src_pos.h>

enum {
	preproc_buf_size = 32,
	preproc_buf_low_watermark = 16,
	preproc_out_buf_size = 32
};

struct preproc_input;

/** C preprocessor state */
typedef enum {
	/** At beginning of line (or after whitespace) */
	pps_line_begin,
	/** Inside a line of text */
	pps_text_line
} preproc_state_t;

/** C preprocessor */
typedef struct preproc {
	/** Input stack (list of preproc_input_t */
	list_t inputs;
	/** Preprocessor state */
	preproc_state_t state;
	/** Currently skipping due to false condition. */
	bool skipping;
	/** Current input */
	struct preproc_input *cur;
	/** Position of beginning of input buffer */
	src_pos_t buf_bpos;
	/** Output buffer */
	char out_buf[preproc_out_buf_size];
	/** Output position buffer */
	src_pos_t out_posbuf[preproc_out_buf_size];
	/** Number of used bytes in out_buf */
	size_t out_buf_used;
	/** Position of beginning of output buffer */
	src_pos_t out_buf_pos;
	/** Directory for standard includes. */
	char *incldir;
	/** Condition stack (list of preproc_condition_t) */
	list_t conditions;
	/** Preprocessor macros (list of preproc_macro_t) */
	list_t macros;
} preproc_t;

/** C preprocessor input stack entry */
typedef struct preproc_input {
	/** Containing C preprocessor */
	preproc_t *preproc;
	/** Link to @c preproc->inputs */
	link_t linputs;
	/** Input file name */
	char *in_fname;
	/** Input file */
	FILE *in_file;
	/** File input */
	file_input_t *finput;
	/** Input ops */
	lexer_input_ops_t *input_ops;
	/** Argument to input ops */
	void *input_arg;
	/** EOF hit in input */
	bool in_eof;
	/** Error hit in input */
	bool in_error;
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
} preproc_input_t;

/** C preprocessor condition stack entry */
typedef struct {
	/** Containing preprocessor */
	struct preproc *preproc;
	/** Link to @c preproc->states */
	link_t lconditions;
	/** Beginning of conditional directive */
	src_pos_t bpos;
	/** End of conditional directive */
	src_pos_t epos;
	/** Encountered an else directive */
	bool has_else;
	/** Beginning of else directive */
	src_pos_t else_bpos;
	/** End of else directive */
	src_pos_t else_epos;
	/** Preprocessor was skipping before entering this condition. */
	bool was_skipping;
} preproc_condition_t;

/** C preprocessor macro */
typedef struct {
	/** Containing preprocessor */
	struct preproc *preproc;
	/** Link to @c preproc->macros */
	link_t lmacros;
	/** Macro name */
	char *name;
} preproc_macro_t;

/** Include type (angle brackets or quotes) */
typedef enum {
	/** Include with angled brackets */
	pit_angled,
	/** Include with quotes */
	pit_quoted
} preproc_include_type_t;

#endif
