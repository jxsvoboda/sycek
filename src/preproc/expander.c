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

#include <assert.h>
#include <input_buf.h>
#include <lexer.h>
#include <preproc/expander.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

static int preproc_expander_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t preproc_expander_input = {
	.read = preproc_expander_read
};

static int preproc_expander_process(preproc_expander_t *);

/** Create preprocessor macro expander.
 *
 * @param inbuf Input buffer
 * @param rexpander Place to store new preprocessor macro expander.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_expander_create(input_buf_t *inbuf, preproc_expander_t **rexpander)
{
	preproc_expander_t *expander = NULL;
	int rc;

	expander = calloc(1, sizeof(preproc_expander_t));
	if (expander == NULL) {
		rc = ENOMEM;
		goto error;
	}

	expander->inbuf = inbuf;

	*rexpander = expander;
	return EOK;
error:
	preproc_expander_destroy(expander);
	return rc;
}

/** Destroy preprocessor macro expander.
 *
 * @param expander Preprocessor macro expander or @c NULL
 */
void preproc_expander_destroy(preproc_expander_t *expander)
{
	if (expander == NULL)
		return;

	free(expander);
}

/** Determine if expander output buffer has available space.
 *
 * @param expander Preprocessor macro expander
 * @return @c true iff output buffer has available space.
 */
static bool preproc_expander_buf_avail(preproc_expander_t *expander)
{
	return expander->out_buf_used < preproc_expander_buf_size;
}

/** Insert character to preprocessor output buffer.
 *
 * @param expander Preprocessor expander
 * @param c Character
 * @param pos Character source code position
 * @return EOK on success or an error code.
 */
static void preproc_expander_buf_insert(preproc_expander_t *expander, char c,
    src_pos_t *pos)
{
	if (expander->out_buf_used == 0)
		expander->out_buf_pos = expander->pos;

	assert(preproc_expander_buf_avail(expander));

	expander->out_buf[expander->out_buf_used] = c;
	expander->out_posbuf[expander->out_buf_used] = *pos;
	++expander->out_buf_used;
}

/** Do some expansion.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param expander Preprocessor expander
 * @return EOK on success or an error code
 */
static int preproc_expander_process(preproc_expander_t *expander)
{
	char c;
	char *p;

	while (!expander->expand_eol && !input_buf_is_eof(expander->inbuf) &&
	    !input_buf_is_error(expander->inbuf)) {
		p = input_buf_chars(expander->inbuf, &expander->pos);
		c = p[0];

		preproc_expander_buf_insert(expander, c, &expander->pos);

		input_buf_advance(expander->inbuf, 1, &expander->pos);
		if (c == '\n') {
			expander->expand_eol = true;
			break;
		}

		if (!preproc_expander_buf_avail(expander))
			break;
	}

	if (input_buf_is_eof(expander->inbuf)) {
		expander->expand_eol = true;
	}

	if (input_buf_is_error(expander->inbuf))
		return EIO;

	return EOK;
}

/** Read input from preprocessor expander.
 *
 * @param arg Argument (preproc_t *)
 * @param buf Character buffer
 * @param bsize Size of character buffer
 * @param nread Place to store number of characters read.
 * @param bpos Place to store position of the beginning of the buffer
 * @return EOK on success or an error code
 */
static int preproc_expander_read(void *arg, char *buf, size_t bsize,
    size_t *nread, src_pos_t *bpos)
{
	preproc_expander_t *expander = (preproc_expander_t *)arg;
	size_t nbytes;
	int rc;

	while (expander->out_buf_used == 0 &&
	    !input_buf_is_error(expander->inbuf) && !expander->expand_eol) {
		rc = preproc_expander_process(expander);
		if (rc != EOK)
			return rc;
	}

	if (input_buf_is_error(expander->inbuf))
		return EIO;

	if (bsize < expander->out_buf_used)
		nbytes = bsize;
	else
		nbytes = expander->out_buf_used;

	memcpy(buf, expander->out_buf, nbytes);
	expander->out_buf_used -= nbytes;

	*bpos = expander->out_posbuf[0];
	*nread = nbytes;

	memcpy(expander->out_buf, expander->out_buf + nbytes,
	    expander->out_buf_used);
	memcpy(expander->out_posbuf, expander->out_posbuf + nbytes,
	    expander->out_buf_used * sizeof(src_pos_t));

	return EOK;
}
