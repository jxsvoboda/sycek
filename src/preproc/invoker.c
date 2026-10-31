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

#include <assert.h>
#include <input_buf.h>
#include <lexer.h>
#include <preproc/invoker.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

static int preproc_invoker_read(void *, char *, src_pos_t *, size_t, size_t *);

lexer_input_ops_t preproc_invoker_input = {
	.read = preproc_invoker_read
};

static int preproc_invoker_process(preproc_invoker_t *);

/** Create preprocessor macro invoker.
 *
 * @param inbuf Input buffer
 * @param rinvoker Place to store new preprocessor macro invoker.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_invoker_create(preproc_macro_t *macro, preproc_invoker_t **rinvoker)
{
	preproc_invoker_t *invoker = NULL;
	int rc;

	invoker = calloc(1, sizeof(preproc_invoker_t));
	if (invoker == NULL) {
		rc = ENOMEM;
		goto error;
	}

	invoker->macro = macro;
	invoker->pos = macro->rpos;

	*rinvoker = invoker;
	return EOK;
error:
	preproc_invoker_destroy(invoker);
	return rc;
}

/** Destroy preprocessor macro invoker.
 *
 * @param invoker Preprocessor macro invoker or @c NULL
 */
void preproc_invoker_destroy(preproc_invoker_t *invoker)
{
	if (invoker == NULL)
		return;

	free(invoker);
}

/** Determine if invoker output buffer has available space.
 *
 * @param invoker Preprocessor macro invoker
 * @return @c true iff output buffer has available space.
 */
static bool preproc_invoker_buf_avail(preproc_invoker_t *invoker)
{
	return invoker->out_buf_used < preproc_invoker_buf_size;
}

/** Insert character to preprocessor output buffer.
 *
 * @param invoker Preprocessor invoker
 * @param c Character
 * @param pos Character source code position
 * @return EOK on success or an error code.
 */
static void preproc_invoker_buf_insert(preproc_invoker_t *invoker, char c,
    src_pos_t *pos)
{
	if (invoker->out_buf_used == 0)
		invoker->out_buf_pos = invoker->pos;

	assert(preproc_invoker_buf_avail(invoker));

	invoker->out_buf[invoker->out_buf_used] = c;
	invoker->out_posbuf[invoker->out_buf_used] = *pos;
	++invoker->out_buf_used;
}

/** Do some macro invoking.
 *
 * This may or may not write data to the invoker output buffer.
 *
 * @param invoker Preprocessor invoker
 * @return EOK on success or an error code
 */
static int preproc_invoker_process(preproc_invoker_t *invoker)
{
	char c;

	while (invoker->macro->replacement[invoker->macro_off] != '\0') {
		c = invoker->macro->replacement[invoker->macro_off++];

		preproc_invoker_buf_insert(invoker, c, &invoker->pos);
		src_pos_fwd_char(&invoker->pos, c);

		if (!preproc_invoker_buf_avail(invoker))
			break;
	}

	return EOK;
}

/** Read input from preprocessor invoker.
 *
 * @param arg Argument (preproc_t *)
 * @param buf Character buffer
 * @param posbuf Position buffer
 * @param bsize Size of character buffer
 * @param nread Place to store number of characters read.
 * @return EOK on success or an error code
 */
static int preproc_invoker_read(void *arg, char *buf, src_pos_t *posbuf,
    size_t bsize, size_t *nread)
{
	preproc_invoker_t *invoker = (preproc_invoker_t *)arg;
	size_t nbytes;
	int rc;

	/// XXX Is this necessary?
	while (invoker->macro->replacement[invoker->macro_off] != '\0' &&
	    preproc_invoker_buf_avail(invoker)) {
		rc = preproc_invoker_process(invoker);
		if (rc != EOK)
			return rc;
	}

	if (bsize < invoker->out_buf_used)
		nbytes = bsize;
	else
		nbytes = invoker->out_buf_used;

	memcpy(buf, invoker->out_buf, nbytes);
	memcpy(posbuf, invoker->out_posbuf, nbytes * sizeof(src_pos_t));
	invoker->out_buf_used -= nbytes;

	*nread = nbytes;

	memcpy(invoker->out_buf, invoker->out_buf + nbytes,
	    invoker->out_buf_used);
	memcpy(invoker->out_posbuf, invoker->out_posbuf + nbytes,
	    invoker->out_buf_used * sizeof(src_pos_t));

	return EOK;
}
