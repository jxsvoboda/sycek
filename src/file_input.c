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
 * Lexer input from file
 */

#include <file_input.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdio.h>

static int file_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_file_input = {
	.read = file_lexer_read
};

/** Lexer input form a string constant. */
static int file_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	file_input_t *finput = (file_input_t *)arg;
	size_t i;
	size_t nr;

	nr = fread(buf, 1, bsize, finput->f);
	if (ferror(finput->f) != 0)
		return EIO;

	*nread = nr;
	*bpos = finput->cpos;

	/* Advance source position */
	for (i = 0; i < nr; i++)
		src_pos_fwd_char(&finput->cpos, buf[i]);

	return EOK;
}

void file_input_init(file_input_t *finput, FILE *f, const char *fname)
{
	finput->f = f;

	src_pos_set(&finput->cpos, fname, 1, 1);
}
