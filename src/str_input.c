/*
 * Copyright 2018 Jiri Svoboda
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
 * Lexer input from string
 *
 * Used for testing
 */

#include <merrno.h>
#include <src_pos.h>
#include <string.h>
#include <str_input.h>

static int str_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_str_input = {
	.read = str_lexer_read
};

/** Lexer input form a string constant. */
static int str_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	str_input_t *sinput = (str_input_t *)arg;
	size_t len;
	size_t i;

	len = strlen(sinput->str + sinput->pos);
	if (bsize < len)
		len = bsize;

	memcpy(buf, sinput->str + sinput->pos, len);
	*nread = len;
	*bpos = sinput->cpos;

	/* Advance source position */
	for (i = 0; i < len; i++)
		src_pos_fwd_char(&sinput->cpos, sinput->str[sinput->pos++]);

	printf("str_lexer_read: pos=");
	src_pos_print_range(&sinput->cpos, &sinput->cpos, stdout);
	printf("\n");

	return EOK;
}

void str_input_init(str_input_t *sinput, const char *s)
{
	sinput->str = s;
	sinput->pos = 0;

	src_pos_set(&sinput->cpos, "none", 1, 1);
}
