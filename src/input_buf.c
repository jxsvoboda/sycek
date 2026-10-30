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

#include <assert.h>
#include <merrno.h>
#include <input_buf.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

/** Create input buffer.
 *
 * @param input_ops Input operations
 * @param input_arg Argument to input operations
 * @param rinbuf Place to store pointer to new input buffer
 * @return EOK on success, ENOMEM if out of memory
 */
int input_buf_create(lexer_input_ops_t *input_ops, void *input_arg,
    input_buf_t **rinbuf)
{
	input_buf_t *inbuf = NULL;
	int rc;

	inbuf = calloc(1, sizeof(input_buf_t));
	if (inbuf == NULL) {
		rc = ENOMEM;
		goto error;
	}

	inbuf->input_ops = input_ops;
	inbuf->input_arg = input_arg;
	*rinbuf = inbuf;
	return EOK;
error:
	return rc;
}

/** Destroy input buffer.
 *
 * @param inbuf Input buffer or @c NULL
 */
void input_buf_destroy(input_buf_t *inbuf)
{
	if (inbuf == NULL)
		return;

	free(inbuf);
}

/** Get valid pointer to characters in input buffer.
 *
 * Returns a pointer into the input buffer, ensuring it contains
 * at least lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @param inbuf Input buffer
 * @param rpos Place to store current position or @c NULL
 * @return Pointer to characters in input buffer.
 */
char *input_buf_chars(input_buf_t *inbuf, src_pos_t *pos)
{
	int rc;
	size_t nread;
	size_t i;
	src_pos_t rpos;

	while (!inbuf->in_eof && inbuf->buf_used -
	    inbuf->buf_pos < input_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(inbuf->buf,
		    inbuf->buf + inbuf->buf_pos,
		    inbuf->buf_used - inbuf->buf_pos);
		memmove(inbuf->posbuf, inbuf->posbuf +
		    inbuf->buf_pos,
		    (inbuf->buf_used - inbuf->buf_pos) *
		    sizeof(src_pos_t));
		inbuf->buf_used -= inbuf->buf_pos;
		inbuf->buf_pos = 0;
		/* XX Advance inbuf->buf_bpos */

		rc = inbuf->input_ops->read(inbuf->input_arg,
		    inbuf->buf + inbuf->buf_used,
		    input_buf_size - inbuf->buf_used, &nread, &rpos);
		if (rc != EOK) {
			inbuf->in_error = true;
			nread = 0;
			rpos = inbuf->pos;
		}

		if (nread == 0)
			inbuf->in_eof = true;
		if (inbuf->buf_used == 0)
			inbuf->pos = rpos;

		for (i = 0; i < nread; i++) {
			inbuf->posbuf[inbuf->buf_used + i] = rpos;
			src_pos_fwd_char(&rpos,
			    inbuf->buf[inbuf->buf_used + i]);
		}

		inbuf->buf_used += nread;
		if (inbuf->buf_used < input_buf_size)
			inbuf->buf[inbuf->buf_used] = '\0';
	}

	if (inbuf->buf_pos >= input_buf_size)
		return NULL;

	if (inbuf->buf_pos < inbuf->buf_used) {
		inbuf->pos =
		    inbuf->posbuf[inbuf->buf_pos];
	}

	if (pos != NULL) {
		*pos = inbuf->posbuf[inbuf->buf_pos];
	}

	return inbuf->buf + inbuf->buf_pos;
}

/** Determine if input buffer is at end of file.
 *
 * @param inbuf Input buffer
 * @return @c true iff there are no more characters available
 */
bool input_buf_is_eof(input_buf_t *inbuf)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = input_buf_chars(inbuf, NULL);
	(void) lc;

	return inbuf->buf_pos == inbuf->buf_used;
}

/** Determine if input buffer hit an error.
 *
 * @param inbuf Input buffer
 * @return @c true iff there was an error while reading input
 */
bool input_buf_is_error(input_buf_t *inbuf)
{
	return inbuf->in_error;
}

/** Get current lexer position in source code.
 *
 * @param inbuf Input buffer
 * @param pos Place to store position
 */
void input_buf_get_pos(input_buf_t *inbuf, src_pos_t *pos)
{
	if (inbuf->buf_pos < inbuf->buf_used) {
		*pos = inbuf->posbuf[inbuf->buf_pos];
	} else if (inbuf->buf_used > 0) {
		*pos = inbuf->posbuf[inbuf->buf_used - 1];
		src_pos_fwd_char(pos, inbuf->buf[inbuf->buf_used - 1]);
	}
}

/** Advance input buffer read position.
 *
 * Advance read position by a certain amount of characters
 *
 * @param inbuf Input buffer
 * @param nchars Number of characters to advance
 */
void input_buf_advance(input_buf_t *inbuf, size_t nchars, src_pos_t *pos)
{
	char *p;

	while (!input_buf_is_eof(inbuf) && /* !input_buf_is_error(inbuf) && */
	    nchars > 0) {
		p = input_buf_chars(inbuf, pos);
		(void)p;

		++inbuf->buf_pos;
		assert(inbuf->buf_pos <= input_buf_size);
		--nchars;
	}
}
