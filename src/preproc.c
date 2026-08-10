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

#include <adt/list.h>
#include <assert.h>
#include <charcls.h>
#include <lexer.h>
#include <preproc.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

static int preproc_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_preproc_input = {
	.read = preproc_lexer_read
};

static preproc_state_entry_t *preproc_push_state(preproc_t *, preproc_state_t);
static preproc_state_entry_t *preproc_top_state(preproc_t *);
static void preproc_pop_state(preproc_t *);

enum {
	/** Size of preprocessor file name buffer. */
	preproc_fname_buf_size = 512
};

/** Create preprocessor.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rpreproc Place to store new preprocessor.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_create(lexer_input_ops_t *input_ops, void *input_arg,
    preproc_t **rpreproc)
{
	preproc_t *preproc = NULL;
	preproc_state_entry_t *entry;
	int rc;

	preproc = calloc(1, sizeof(preproc_t));
	if (preproc == NULL) {
		rc = ENOMEM;
		goto error;
	}

	preproc->input_ops = input_ops;
	preproc->input_arg = input_arg;

	list_initialize(&preproc->states);

	entry = preproc_push_state(preproc, pps_line_begin);
	if (entry == NULL) {
		rc = ENOMEM;
		goto error;
	}

	*rpreproc = preproc;
	return EOK;
error:
	if (preproc != NULL)
		free(preproc);
	return rc;
}

/** Destroy preprocessor.
 *
 * @param preproc Preprocessor or @c NULL
 */
void preproc_destroy(preproc_t *preproc)
{
	preproc_state_entry_t *entry;

	if (preproc == NULL)
		return;

	entry = preproc_top_state(preproc);
	while (entry != NULL) {
		preproc_pop_state(preproc);

		entry = preproc_top_state(preproc);
	}

	free(preproc);
}

/** Return current preprocessor state entry (from top of state stack).
 *
 * @param preproc Preprocessor
 * @return Topmost state entry or @c NULL if there are none
 */
static preproc_state_entry_t *preproc_top_state(preproc_t *preproc)
{
	link_t *link;

	link = list_first(&preproc->states);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_state_entry_t, lstates);
}

/** Push state entry to preprocessor state stack.
 *
 * @param preproc Preprocessor
 * @param state Preprocessor state
 * @return New state entry (the user may need to fill in details)
 */
static preproc_state_entry_t *preproc_push_state(preproc_t *preproc,
    preproc_state_t state)
{
	preproc_state_entry_t *entry;

	entry = calloc(1, sizeof(preproc_state_entry_t));
	if (entry == NULL)
		return NULL;

	entry->state = state;
	list_append(&entry->lstates, &preproc->states);
	return entry;
}

/** Pop state entry from preprocessor state stack.
 *
 * @param preproc Preprocessor
 */
static void preproc_pop_state(preproc_t *preproc)
{
	preproc_state_entry_t *entry;

	entry = preproc_top_state(preproc);
	list_remove(&entry->lstates);
	free(entry);
}

/** Get valid pointer to characters in input buffer.
 *
 * Returns a pointer into the input buffer, ensuring it contains
 * at least lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @param preproc Preprocessor
 * @return Pointer to characters in input buffer.
 */
static char *preproc_chars(preproc_t *preproc)
{
	int rc;
	size_t nread;
	size_t i;
	src_pos_t rpos;

	while (!preproc->in_eof && preproc->buf_used - preproc->buf_pos <
	    preproc_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(preproc->buf, preproc->buf + preproc->buf_pos,
		    preproc->buf_used - preproc->buf_pos);
		memmove(preproc->posbuf, preproc->posbuf + preproc->buf_pos,
		    (preproc->buf_used - preproc->buf_pos) * sizeof(src_pos_t));
		preproc->buf_used -= preproc->buf_pos;
		preproc->buf_pos = 0;
		/* XX Advance preproc->buf_bpos */

		rc = preproc->input_ops->read(preproc->input_arg, preproc->buf +
		    preproc->buf_used, preproc_buf_size - preproc->buf_used,
		    &nread, &rpos);
		if (rc != EOK) {
			preproc->in_error = true;
			nread = 0;
			rpos = preproc->pos;
		}

		if (nread == 0)
			preproc->in_eof = true;
		if (preproc->buf_used == 0) {
			preproc->buf_bpos = rpos;
			preproc->pos = rpos;
		}

		for (i = 0; i < nread; i++) {
			preproc->posbuf[preproc->buf_used + i] = rpos;
			src_pos_fwd_char(&rpos,
			    preproc->buf[preproc->buf_used + i]);
		}

		preproc->buf_used += nread;
		if (preproc->buf_used < preproc_buf_size)
			preproc->buf[preproc->buf_used] = '\0';
	}

	assert(preproc->buf_pos < preproc_buf_size);
	return preproc->buf + preproc->buf_pos;
}

/** Determine if preprocessor is at end of file.
 *
 * @param preproc Preprocessor
 * @return @c true iff there are no more characters available
 */
static bool preproc_is_eof(preproc_t *preproc)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = preproc_chars(preproc);
	(void) lc;

	return preproc->buf_pos == preproc->buf_used;
}

/** Determine if preprocessor hit an error.
 *
 * @param preproc Preprocessor
 * @return @c true iff there was an error while reading input
 */
static bool preproc_is_error(preproc_t *preproc)
{
	return preproc->in_error;
}

/** Get current preprocessor position in source code.
 *
 * @param preproc Preprocessor
 * @param pos Place to store position
 */
static void preproc_get_pos(preproc_t *preproc, src_pos_t *pos)
{
	if (preproc->buf_pos < preproc->buf_used) {
		*pos = preproc->posbuf[preproc->buf_pos];
	} else if (preproc->buf_used > 0) {
		*pos = preproc->posbuf[preproc->buf_used - 1];
		src_pos_fwd_char(pos, preproc->buf[preproc->buf_used - 1]);
	}
}

/** Advance preprocessor read position.
 *
 * Advance read position by a certain amount of characters
 *
 * @param preproc Preprocessor
 * @param nchars Number of characters to advance
 *
 * @return EOK on success or non-zero error code
 */
static int preproc_advance(preproc_t *preproc, size_t nchars)
{
	char *p;

	while (nchars > 0) {
		p = preproc_chars(preproc);
		(void)p;

		if (preproc->buf_pos < preproc->buf_used)
			preproc->pos = preproc->posbuf[preproc->buf_pos];

		++preproc->buf_pos;
		assert(preproc->buf_pos <= preproc_buf_size);
		--nchars;
	}

	return EOK;
}

/** Print source range for diagnostics.
 *
 * @param bpos Begin position
 * @param epos End position
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int preproc_dprint_range(src_pos_t *bpos, src_pos_t *epos, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(bpos, epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ">") < 0)
		return EIO;

	return EOK;
}

/** Print error expected <header-name> or "header-name".
 *
 * @param preproc Preprocessor
 */
static void preproc_error_header_name(preproc_t *preproc)
{
	(void)preproc_dprint_range(&preproc->pos, &preproc->pos, stderr);
	(void)fprintf(stderr, ": Expected <header-name> or \"header-name\".\n");
}

/** Process whitespace.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_ws(preproc_t *preproc)
{
	char *p;

	/* Skip whitespace. */
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);

		if (p[0] != ' ' && p[0] != '\t')
			break;

		preproc_advance(preproc, 1);
	}

	return preproc_is_error(preproc) ? EIO : EOK;
}

/** Process whitespace + end of line.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_ws_eol(preproc_t *preproc)
{
	char *p;
	int rc;

	rc = preproc_process_ws(preproc);
	if (rc != EOK)
		return rc;

	if (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (p[0] != '\n') {
			(void)preproc_dprint_range(&preproc->pos, &preproc->pos,
			    stderr);
			(void)fprintf(stderr, ": Unexpected characters "
			    "at end of line.\n");
			return EINVAL;
		}
	}

	if (preproc_is_error(preproc))
		return EIO;

	preproc_advance(preproc, 1);
	return EOK;
}

/** Process invalid directive and print diagnostics.
 *
 * @param preproc Preprocessor
 */
static void preproc_process_invalid_directive(preproc_t *preproc)
{
	src_pos_t spos;
	char *p;

	spos = preproc->pos;
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (!is_idcnt(p[0]))
			break;

		preproc_advance(preproc, 1);
	}

	(void)preproc_dprint_range(&spos, &preproc->pos, stderr);
	(void)fprintf(stderr, ": Invalid preprocessor directive.\n");
}

/** Process include directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_include(preproc_t *preproc)
{
	char *p;
	char delim;
	char *fname;
	size_t buf_pos;
	int rc;

	fname = calloc(preproc_fname_buf_size, 1);
	if (fname == NULL)
		return ENOMEM;

	preproc_advance(preproc, 7);

	rc = preproc_process_ws(preproc);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_header_name(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	p = preproc_chars(preproc);
	switch (p[0]) {
	case '<':
		delim = '>';
		break;
	case '"':
		delim = '"';
		break;
	default:
		preproc_error_header_name(preproc);
		rc = EINVAL;
		goto error;
	}

	buf_pos = 0;
	preproc_advance(preproc, 1);
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (p[0] == delim || p[0] == '\n')
			break;

		if (buf_pos >= preproc_fname_buf_size - 1) {
			printf("Include filename too long.\n");
			rc = EINVAL;
			goto error;
		}

		fname[buf_pos++] = p[0];
		preproc_advance(preproc, 1);
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	if (p[0] != delim) {
		(void)preproc_dprint_range(&preproc->pos, &preproc->pos, stderr);
		(void)fprintf(stderr, ": Missing terminating '%c' "
		    "character.\n", delim);
		rc = EINVAL;
		goto error;
	}

	fname[buf_pos] = '\0';
	preproc_advance(preproc, 1);

	rc = preproc_process_ws_eol(preproc);
	if (rc != EOK)
		goto error;

	if (delim == '>')
		printf("Include file '%s' with '<>'.\n", fname);
	else
		printf("Include file '%s' with '\"\"'.\n", fname);

	free(fname);
	return EOK;
error:
	free(fname);
	return rc;
}

/** Process a directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_directive(preproc_t *preproc)
{
	char *p;
	int rc;

	/* Skip '#'. */
	preproc_advance(preproc, 1);

	/* Skip whitespace. */
	rc = preproc_process_ws(preproc);
	if (rc != EOK)
		return rc;

	p = preproc_chars(preproc);

	switch (p[0]) {
	case 'i':
		if (p[1] == 'n' && p[2] == 'c' && p[3] == 'l' &&
		    p[4] == 'u' && p[5] == 'd' && p[6] == 'e' &&
		    !is_idcnt(p[7])) {
			rc = preproc_process_include(preproc);
		}
		break;
	default:
		printf("Invalid directive.\n");
		preproc_process_invalid_directive(preproc);
		rc = EINVAL;
	}

	return rc;
}

/** Process in line begin state.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_line_begin(preproc_t *preproc)
{
	char *p;
	size_t ws_cnt;
	int rc;

	(void)preproc_get_pos;//XXX

	/* Process whitespace at begining of line. */

	ws_cnt = 0;
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (p[0] != ' ' && p[0] != '\t')
			break;

		++ws_cnt;
		preproc_advance(preproc, 1);
	}

	if (preproc_is_error(preproc))
		return EIO;

	if (preproc_is_eof(preproc))
		return EOK;

	p = preproc_chars(preproc);
	if (p[0] == '#') {
		/* Preprocessor directive. */
		rc = preproc_process_directive(preproc);
		if (rc != EOK)
			return rc;
	} else {
		/* Text line. */
		preproc_pop_state(preproc);
		preproc_push_state(preproc, pps_text_line);
	}

	return EOK;
}

/** Process in text line state.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_text_line(preproc_t *preproc)
{
	char *p;

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);

		preproc->out_buf[preproc->out_buf_used++] = p[0];
		if (preproc->out_buf_used >= preproc_out_buf_size)
			break;
		if (p[0] == '\n') {
			preproc_advance(preproc, 1);
			preproc_pop_state(preproc);
			preproc_push_state(preproc, pps_line_begin);
			break;
		}

		preproc_advance(preproc, 1);
	}

	if (preproc_is_error(preproc))
		return EIO;

	return EOK;
}

/** Process some input.
 *
 * This may or may not write data to the output buffer.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code
 */
static int preproc_process(preproc_t *preproc)
{
	preproc_state_entry_t *entry;

	entry = preproc_top_state(preproc);
	switch (entry->state) {
	case pps_line_begin:
		return preproc_process_line_begin(preproc);
	case pps_text_line:
		return preproc_process_text_line(preproc);
	}

	return EINVAL;
}

/** Lexer input from preprocessor.
 *
 * @param arg Argument (preproc_t *)
 * @param buf Character buffer
 * @param bsize Size of character buffer
 * @param nread Place to store number of characters read.
 * @param bpos Place to store position of the beginning of the buffer
 * @return EOK on success or an error code
 */
static int preproc_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	preproc_t *preproc = (preproc_t *)arg;
	size_t nbytes;
	int rc;

	while (preproc->out_buf_used == 0 && !preproc_is_eof(preproc) &&
	    !preproc_is_error(preproc)) {
		rc = preproc_process(preproc);
		if (rc != EOK)
			return rc;
	}

	if (preproc_is_error(preproc))
		return EIO;

	if (bsize < preproc->out_buf_used)
		nbytes = bsize;
	else
		nbytes = preproc->out_buf_used;

	memcpy(buf, preproc->out_buf, nbytes);
	preproc->out_buf_used = 0;
	*bpos = preproc->out_buf_pos;
	*nread = nbytes;

	return EOK;
}
