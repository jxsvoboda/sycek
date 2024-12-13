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
 * Parser
 */

#ifndef TYPES_PARSER_H
#define TYPES_PARSER_H

#include <types/lexer.h>

/** Parser input ops */
typedef struct {
	/** Read input token */
	void (*read_tok)(void *, void *, unsigned, bool, lexer_tok_t *);
	/** Return next token */
	void *(*next_tok)(void *, void *);
	/** Get data that should be stored into AST for a token */
	void *(*tok_data)(void *, void *);
} parser_input_ops_t;

/** Parser callbacks.
 *
 * Used for parsing the source file in steps. This is required when
 * compiling, so that we have knowledge of prior type definitions.
 */
typedef struct {
	/** Process global declaration. */
	int (*process_global_decln)(void *, ast_node_t **);
} parser_cb_t;

/** Parser */
typedef struct parser {
	/** Input ops */
	parser_input_ops_t *input_ops;
	/** Input argument */
	void *input_arg;
	/** Callbacks for per-parts parsing or @c NULL if not required. */
	parser_cb_t *cb;
	/** Callback argument */
	void *cb_arg;
	/** Next token */
	void *tok;
	/** @c true to supress error messages */
	bool silent;
	/** Current indentation level */
	unsigned indlvl;
	/** Currently in secondary continuation? */
	bool seccont;
} parser_t;

#endif
