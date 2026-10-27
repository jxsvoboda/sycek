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
#include <charcls.h>
#include <input_buf.h>
#include <lexer.h>
#include <preproc/expander.h>
#include <preproc/invoker.h>
#include <merrno.h>
#include <preproc/preproc.h>
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
 * @param preproc Preprocessor
 * @param inbuf Input buffer
 * @param rexpander Place to store new preprocessor macro expander.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_expander_create(preproc_t *preproc, input_buf_t *inbuf,
    preproc_expander_t **rexpander)
{
	preproc_expander_t *expander = NULL;
	int rc;

	expander = calloc(1, sizeof(preproc_expander_t));
	if (expander == NULL) {
		rc = ENOMEM;
		goto error;
	}

	expander->preproc = preproc;
	expander->inbuf = inbuf;
	expander->state = ppes_scan;

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

	if (expander->minbuf != NULL)
		input_buf_destroy(expander->minbuf);
	if (expander->invoker != NULL)
		preproc_invoker_destroy(expander->invoker);

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

/** Handle identifier found in the input stream.
 *
 * @param expander Macro expander
 * @return EOK on success or an error code
 */
static int preproc_expander_handle_ident(preproc_expander_t *expander)
{
	preproc_macro_t *macro;
	int rc;

	macro = preproc_macro_find(expander->preproc, expander->ident_buf);

	if (macro != NULL) {
		/* Defined macro. */
		rc = preproc_invoker_create(macro, &expander->invoker);
		if (rc != EOK)
			return rc;

		rc = input_buf_create(&preproc_invoker_input,
		    expander->invoker, &expander->minbuf);
		if (rc != EOK) {
			preproc_invoker_destroy(expander->invoker);
			expander->invoker = false;
			return rc;
		}

		expander->state = ppes_macro;
	} else {
		/* Unknown idntifier. */
		expander->state = ppes_unkident;
		expander->ident_buf_off = 0;
	}

	return EOK;
}

/** Do some expansion in scan state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param expander Preprocessor expander
 * @return EOK on success or an error code
 */
static int preproc_expander_process_scan(preproc_expander_t *expander)
{
	char c;
	char *p;

	while (!expander->expand_eol && !input_buf_is_eof(expander->inbuf) &&
	    !input_buf_is_error(expander->inbuf)) {
		p = input_buf_chars(expander->inbuf, &expander->pos);
		c = p[0];

		if (is_idbegin(c)) {
			expander->state = ppes_ident;
			expander->ident_buf_used = 0;
			break;
		}

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

/** Do some expansion in identifier state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param expander Preprocessor expander
 * @return EOK on success or an error code
 */
static int preproc_expander_process_ident(preproc_expander_t *expander)
{
	char c;
	char *p;
	int rc;
	src_pos_t pos;

	while (!expander->expand_eol && !input_buf_is_eof(expander->inbuf) &&
	    !input_buf_is_error(expander->inbuf)) {
		p = input_buf_chars(expander->inbuf, &pos);
		c = p[0];

		if (!is_idcnt(c)) {
			expander->ident_buf[expander->ident_buf_used] = '\0';
			rc = preproc_expander_handle_ident(expander);
			if (rc != EOK)
				return rc;
			break;
		}

		if (expander->ident_buf_used ==
		    preproc_expander_ident_buf_size - 1) {
			/* Identifier too long. */
			expander->state = ppes_unkident;
			expander->ident_buf_off = 0;
			break;
		}

		expander->ident_buf[expander->ident_buf_used++] = c;

		input_buf_advance(expander->inbuf, 1, &pos);
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

/** Do some expansion in unknown identifier dumping state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param expander Preprocessor expander
 * @return EOK on success or an error code
 */
static int preproc_expander_process_unkident(preproc_expander_t *expander)
{
	char c;

	while (expander->ident_buf_off < expander->ident_buf_used) {
		c = expander->ident_buf[expander->ident_buf_off++];

		preproc_expander_buf_insert(expander, c, &expander->pos);
		src_pos_fwd_char(&expander->pos, c);

		if (!preproc_expander_buf_avail(expander))
			break;
	}

	if (expander->ident_buf_off >= expander->ident_buf_used)
		expander->state = ppes_scan;

	return EOK;
}

/** Do some expansion in macro invoking state.
 *
 * This may or may not write data to the expander output buffer.
 *
 * @param expander Preprocessor expander
 * @return EOK on success or an error code
 */
static int preproc_expander_process_macro(preproc_expander_t *expander)
{
	char c;
	char *p;

	while (!input_buf_is_eof(expander->minbuf) &&
	    !input_buf_is_error(expander->minbuf)) {
		p = input_buf_chars(expander->minbuf, &expander->pos);
		c = p[0];

		preproc_expander_buf_insert(expander, c, &expander->pos);

		input_buf_advance(expander->minbuf, 1, &expander->pos);

		if (!preproc_expander_buf_avail(expander))
			break;
	}

	if (input_buf_is_eof(expander->minbuf)) {
		input_buf_destroy(expander->minbuf);
		expander->minbuf = NULL;

		preproc_invoker_destroy(expander->invoker);
		expander->invoker = NULL;

		expander->state = ppes_scan;
	}

	if (input_buf_is_error(expander->inbuf))
		return EIO;

	return EOK;
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
	switch (expander->state) {
	case ppes_scan:
		return preproc_expander_process_scan(expander);
	case ppes_ident:
		return preproc_expander_process_ident(expander);
	case ppes_unkident:
		return preproc_expander_process_unkident(expander);
	case ppes_macro:
		return preproc_expander_process_macro(expander);
	}

	return EINVAL;
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
