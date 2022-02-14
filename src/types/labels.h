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
 * Labels
 */

#ifndef TYPES_LABELS_H
#define TYPES_LABELS_H

#include <adt/list.h>
#include <types/lexer.h>

/** Labels */
typedef struct labels {
	/** List of labels */
	list_t labels; /* of label_t */
} labels_t;

/** Label */
typedef struct {
	/** Containing labels */
	labels_t *labels;
	/** Link to labels_t.labels */
	link_t llabels;
	/** Identifier token */
	lexer_tok_t *tident;
	/** @c true iff label is defined */
	bool defined;
	/** @c true iff label is used */
	bool used;
} label_t;

#endif
