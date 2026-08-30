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
#include <file_input.h>
#include <lexer.h>
#include <pathname.h>
#include <preproc.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdlib.h>
#include <string.h>

static int preproc_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_preproc_input = {
	.read = preproc_lexer_read
};

static int preproc_expand_process(preproc_t *);
static int preproc_push_input(preproc_t *, const char *, FILE *, file_input_t *,
    lexer_input_ops_t *, void *);
static void preproc_pop_input(preproc_t *);
static preproc_condition_t *preproc_push_condition(preproc_t *,
    src_pos_t *, src_pos_t *, bool);
static preproc_condition_t *preproc_top_condition(preproc_t *);
static void preproc_pop_condition(preproc_t *);
static preproc_macro_t *preproc_macro_first(preproc_t *);
static void preproc_macro_remove(preproc_macro_t *);
static preproc_macro_t *preproc_macro_find(preproc_t *, const char *);
static int preproc_skip_to_end_of_line(preproc_t *);
static int preproc_dump_to_end_of_line(preproc_t *, FILE *);

enum {
	/** Size of preprocessor file name buffer. */
	preproc_fname_buf_size = 512,

	/** Size of macro name buffer. */
	preproc_macro_name_buf_size = 128
};

/** Create preprocessor.
 *
 * @param fname File name
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rpreproc Place to store new preprocessor.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int preproc_create(const char *fname, lexer_input_ops_t *input_ops,
    void *input_arg, preproc_t **rpreproc)
{
	preproc_t *preproc = NULL;
	int rc;

	preproc = calloc(1, sizeof(preproc_t));
	if (preproc == NULL) {
		rc = ENOMEM;
		goto error;
	}

	list_initialize(&preproc->inputs);
	list_initialize(&preproc->macros);

	rc = preproc_push_input(preproc, fname, NULL, NULL, input_ops,
	    input_arg);
	if (rc != EOK)
		goto error;

	preproc->state = pps_line_begin;

	*rpreproc = preproc;
	return EOK;
error:
	preproc_destroy(preproc);
	return rc;
}

/** Destroy preprocessor.
 *
 * @param preproc Preprocessor or @c NULL
 */
void preproc_destroy(preproc_t *preproc)
{
	preproc_macro_t *macro;

	if (preproc == NULL)
		return;

	while (preproc->cur != NULL)
		preproc_pop_input(preproc);

	macro = preproc_macro_first(preproc);
	while (macro != NULL) {
		preproc_macro_remove(macro);
		macro = preproc_macro_first(preproc);
	}

	if (preproc->incldir != NULL)
		free(preproc->incldir);

	free(preproc);
}

/** Set include directory for standard headers.
 *
 * @param preproc Preprocessor
 * @param dir Dirctory
 * @return EOK on success or an error code
 */
int preproc_set_incldir(preproc_t *preproc, const char *dir)
{
	char *ddir;

	ddir = strdup(dir);
	if (ddir == NULL)
		return ENOMEM;

	if (preproc->incldir != NULL)
		free(preproc->incldir);
	preproc->incldir = ddir;
	return EOK;
}

/** Return innermost preprocessor condition directive.
 *
 * @param preproc Preprocessor
 * @return Topmost condition or @c NULL if there are none
 */
static preproc_condition_t *preproc_top_condition(preproc_t *preproc)
{
	link_t *link;

	link = list_last(&preproc->cur->conditions);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_condition_t, lconditions);
}

/** Push entry to preprocessor input stack.
 *
 * @param preproc Preprocessor
 * @param fname Input file name or @c NULL
 * @param file Input file or @c NULL
 * @param finput File input or @c NULL
 * @param input_ops Input operations
 * @param input_arg Argument to input ops
 * @return EOK on success, ENOMEM if out of memory
 */
static int preproc_push_input(preproc_t *preproc, const char *fname, FILE *file,
    file_input_t *finput, lexer_input_ops_t *input_ops, void *input_arg)
{
	preproc_input_t *input;

	input = calloc(1, sizeof(preproc_input_t));
	if (input == NULL)
		return ENOMEM;

	if (fname != NULL) {
		input->in_fname = strdup(fname);
		if (input->in_fname == NULL) {
			free(input);
			return ENOMEM;
		}
	}

	input->in_file = file;
	input->finput = finput;
	input->input_ops = input_ops;
	input->input_arg = input_arg;
	list_append(&input->linputs, &preproc->inputs);
	list_initialize(&input->conditions);

	preproc->cur = input;
	return EOK;
}

/** Pop entry from preprocessor input stack.
 *
 * @param preproc Preprocessor
 */
static void preproc_pop_input(preproc_t *preproc)
{
	preproc_condition_t *condition;
	link_t *link;

	if (preproc == NULL)
		return;

	condition = preproc_top_condition(preproc);
	while (condition != NULL) {
		preproc_pop_condition(preproc);

		condition = preproc_top_condition(preproc);
	}

	list_remove(&preproc->cur->linputs);
	if (preproc->cur->finput != NULL)
		file_input_destroy(preproc->cur->finput);
	if (preproc->cur->in_file != NULL)
		(void)fclose(preproc->cur->in_file);
	if (preproc->cur->in_fname != NULL)
		free(preproc->cur->in_fname);
	free(preproc->cur);

	link = list_last(&preproc->inputs);
	if (link != NULL) {
		preproc->cur = list_get_instance(link, preproc_input_t,
		    linputs);
	} else {
		preproc->cur = NULL;
	}
}

/** Push condition to preprocessor condition stack.
 *
 * @param preproc Preprocessor
 * @param bpos Position of the beginning
 * @param epos Position of the end
 * @param was_skipping Preprocess was skipping before entering condition
 * @return New condition or @c NULL if out of memory
 */
static preproc_condition_t *preproc_push_condition(preproc_t *preproc,
    src_pos_t *bpos, src_pos_t *epos, bool was_skipping)
{
	preproc_condition_t *condition;

	condition = calloc(1, sizeof(preproc_condition_t));
	if (condition == NULL)
		return NULL;

	condition->preproc = preproc;
	list_append(&condition->lconditions, &preproc->cur->conditions);
	condition->bpos = *bpos;
	condition->epos = *epos;
	condition->was_skipping = was_skipping;
	return condition;
}

/** Pop condition from preprocessor condition stack.
 *
 * @param preproc Preprocessor
 */
static void preproc_pop_condition(preproc_t *preproc)
{
	preproc_condition_t *condition;

	condition = preproc_top_condition(preproc);
	list_remove(&condition->lconditions);
	free(condition);
}

/** Add new preprocessor macro.
 *
 * @param preproc Preprocessor
 * @param name Macro name
 * @param replacement Replacement text
 * @return EOK on success or an error code.
 */
static int preproc_macro_add(preproc_t *preproc, const char *name,
    const char *replacement)
{
	preproc_macro_t *macro;

	macro = calloc(1, sizeof(preproc_macro_t));
	if (macro == NULL)
		return ENOMEM;

	macro->name = strdup(name);
	if (macro->name == NULL) {
		free(macro);
		return ENOMEM;
	}

	macro->replacement = strdup(replacement);
	if (macro->replacement == NULL) {
		free(macro->name);
		free(macro);
		return ENOMEM;
	}

	macro->preproc = preproc;
	list_append(&macro->lmacros, &preproc->macros);
	return EOK;
}

/** Return fist preprocessor macro.
 *
 * @param preproc Preprocessor
 * @return First macro or @c NULL if there are none
 */
static preproc_macro_t *preproc_macro_first(preproc_t *preproc)
{
	link_t *link;

	link = list_first(&preproc->macros);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_macro_t, lmacros);
}

/** Return next preprocessor macro.
 *
 * @param cur Current macro
 * @return First macro or @c NULL if @a cur was the last.
 */
static preproc_macro_t *preproc_macro_next(preproc_macro_t *cur)
{
	link_t *link;

	link = list_next(&cur->lmacros, &cur->preproc->macros);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, preproc_macro_t, lmacros);
}

/** Find preprocessor macro by name.
 *
 * @param preproc Preprocessor
 * @param name Macro name
 * @return Macro or @c NULL if not found.
 */
static preproc_macro_t *preproc_macro_find(preproc_t *preproc, const char *name)
{
	preproc_macro_t *macro;

	macro = preproc_macro_first(preproc);
	while (macro != NULL) {
		if (strcmp(macro->name, name) == 0)
			return macro;

		macro = preproc_macro_next(macro);
	}

	return NULL;
}

/** Remove preprocessor macro.
 *
 * @param macro Preprocessor macro
 */
static void preproc_macro_remove(preproc_macro_t *macro)
{
	list_remove(&macro->lmacros);
	free(macro->name);
	free(macro->replacement);
	free(macro);
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

	while (!preproc->cur->in_eof && preproc->cur->buf_used -
	    preproc->cur->buf_pos < preproc_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(preproc->cur->buf,
		    preproc->cur->buf + preproc->cur->buf_pos,
		    preproc->cur->buf_used - preproc->cur->buf_pos);
		memmove(preproc->cur->posbuf, preproc->cur->posbuf +
		    preproc->cur->buf_pos,
		    (preproc->cur->buf_used - preproc->cur->buf_pos) *
		    sizeof(src_pos_t));
		preproc->cur->buf_used -= preproc->cur->buf_pos;
		preproc->cur->buf_pos = 0;
		/* XX Advance preproc->buf_bpos */

		rc = preproc->cur->input_ops->read(preproc->cur->input_arg,
		    preproc->cur->buf + preproc->cur->buf_used,
		    preproc_buf_size - preproc->cur->buf_used, &nread, &rpos);
		if (rc != EOK) {
			preproc->cur->in_error = true;
			nread = 0;
			rpos = preproc->cur->pos;
		}

		if (nread == 0)
			preproc->cur->in_eof = true;
		if (preproc->cur->buf_used == 0) {
			preproc->buf_bpos = rpos;
			preproc->cur->pos = rpos;
		}

		for (i = 0; i < nread; i++) {
			preproc->cur->posbuf[preproc->cur->buf_used + i] = rpos;
			src_pos_fwd_char(&rpos,
			    preproc->cur->buf[preproc->cur->buf_used + i]);
		}

		preproc->cur->buf_used += nread;
		if (preproc->cur->buf_used < preproc_buf_size)
			preproc->cur->buf[preproc->cur->buf_used] = '\0';
	}

	assert(preproc->cur->buf_pos < preproc_buf_size);

	if (preproc->cur->buf_pos < preproc->cur->buf_used) {
		preproc->cur->pos =
		    preproc->cur->posbuf[preproc->cur->buf_pos];
	}

	return preproc->cur->buf + preproc->cur->buf_pos;
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

	return preproc->cur->buf_pos == preproc->cur->buf_used;
}

/** Determine if preprocessor hit an error.
 *
 * @param preproc Preprocessor
 * @return @c true iff there was an error while reading input
 */
static bool preproc_is_error(preproc_t *preproc)
{
	return preproc->cur->in_error;
}

/** Advance preprocessor read position.
 *
 * Advance read position by a certain amount of characters
 *
 * @param preproc Preprocessor
 * @param nchars Number of characters to advance
 */
static void preproc_advance(preproc_t *preproc, size_t nchars)
{
	char *p;

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc) &&
	    nchars > 0) {
		p = preproc_chars(preproc);
		(void)p;

		++preproc->cur->buf_pos;
		assert(preproc->cur->buf_pos <= preproc_buf_size);
		--nchars;
	}
}

/** Determine if output buffer has available space.
 *
 * @param preproc Preprocessor
 * @return @c true iff output buffer has available space.
 */
static bool preproc_out_buf_avail(preproc_t *preproc)
{
	return preproc->out_buf_used < preproc_out_buf_size;
}

/** Get valid pointer to characters in expansion buffer.
 *
 * Returns a pointer into the expansion buffer, ensuring it contains
 * at least lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @param preproc Preprocessor
 * @return Pointer to characters in input buffer.
 */
static char *preproc_xbuf_chars(preproc_t *preproc)
{
	int rc;

	while (!preproc->expand_eol && !preproc_is_eof(preproc) &&
	    preproc->xbuf_used - preproc->xbuf_pos < preproc_xbuf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(preproc->xbuf,
		    preproc->xbuf + preproc->xbuf_pos,
		    preproc->xbuf_used - preproc->xbuf_pos);
		memmove(preproc->xposbuf, preproc->xposbuf +
		    preproc->xbuf_pos,
		    (preproc->xbuf_used - preproc->xbuf_pos) *
		    sizeof(src_pos_t));
		preproc->xbuf_used -= preproc->xbuf_pos;
		preproc->xbuf_pos = 0;
		/* XX Advance preproc->buf_bpos */

		/* Do some expanding */
		rc = preproc_expand_process(preproc);
		if (rc != EOK) {
			preproc->xbuf_in_error = true;
			break;
		}

		if (preproc->xbuf_used < preproc_xbuf_size)
			preproc->xbuf[preproc->xbuf_used] = '\0';
	}

	if (preproc->xbuf_pos < preproc->xbuf_used) {
		preproc->cur->pos =
		    preproc->xposbuf[preproc->xbuf_pos];
	}
	return preproc->xbuf + preproc->xbuf_pos;
}

/** Determine if preprocessor is at end of file.
 *
 * @param preproc Preprocessor
 * @return @c true iff there are no more characters available
 */
static bool preproc_xbuf_is_eof(preproc_t *preproc)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = preproc_xbuf_chars(preproc);
	(void) lc;

	return preproc->xbuf_pos == preproc->xbuf_used;
}

/** Determine if preprocessor expander hit an error.
 *
 * @param preproc Preprocessor
 * @return @c true iff there was an error while reading input
 */
static bool preproc_xbuf_is_error(preproc_t *preproc)
{
	return preproc->xbuf_in_error;
}

/** Advance preprocessor expansion buffer read position.
 *
 * Advance read position by a certain amount of characters
 *
 * @param preproc Preprocessor
 * @param nchars Number of characters to advance
 */
static void preproc_xbuf_advance(preproc_t *preproc, size_t nchars)
{
	char *p;

	while (!preproc_xbuf_is_eof(preproc) &&
	    !preproc_is_error(preproc) && nchars > 0) {
		p = preproc_xbuf_chars(preproc);
		(void)p;

		++preproc->xbuf_pos;
		assert(preproc->xbuf_pos <= preproc_xbuf_size);
		--nchars;
	}
}

/** Determine if expansion buffer has available space.
 *
 * @param preproc Preprocessor
 * @return @c true iff expansion buffer has available space.
 */
static bool preproc_xbuf_avail(preproc_t *preproc)
{
	return preproc->xbuf_used < preproc_xbuf_size;
}

/** Insert character to preprocessor expansion buffer.
 *
 * @param preproc Preprocessor
 * @param c Character
 * @param pos Character source code position
 * @return EOK on success or an error code.
 */
static void preproc_xbuf_insert(preproc_t *preproc, char c, src_pos_t *pos)
{
	assert(preproc_xbuf_avail(preproc));

	preproc->xbuf[preproc->xbuf_used] = c;
	preproc->xposbuf[preproc->xbuf_used] = *pos;
	++preproc->xbuf_used;
}

/** Insert character to preprocessor output buffer.
 *
 * @param preproc Preprocessor
 * @param c Character
 * @param pos Character source code position
 * @return EOK on success or an error code.
 */
static void preproc_out_buf_insert(preproc_t *preproc, char c, src_pos_t *pos)
{
	if (preproc->out_buf_used == 0)
		preproc->out_buf_pos = preproc->cur->pos;

	assert(preproc_out_buf_avail(preproc));

	preproc->out_buf[preproc->out_buf_used] = c;
	preproc->out_posbuf[preproc->out_buf_used] = *pos;
	++preproc->out_buf_used;
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
	(void)preproc_dprint_range(&preproc->cur->pos, &preproc->cur->pos,
	    stderr);
	(void)fprintf(stderr, ": Expected <header-name> or \"header-name\".\n");
}

/** Print error expected macro name.
 *
 * @param preproc Preprocessor
 */
static void preproc_error_macro_name(preproc_t *preproc)
{
	(void)preproc_dprint_range(&preproc->cur->pos, &preproc->cur->pos,
	    stderr);
	(void)fprintf(stderr, ": Expected macro name.\n");
}

/** Print error expected condition.
 *
 * @param preproc Preprocessor
 */
static void preproc_error_condition(preproc_t *preproc)
{
	(void)preproc_dprint_range(&preproc->cur->pos, &preproc->cur->pos,
	    stderr);
	(void)fprintf(stderr, ": Expected condition.\n");
}

/** Process whitespace.
 *
 * @param preproc Preprocessor
 * @param rws Place to store @c true iff any whitespace characters were
 *            processed or @c NULL if not interested.
 * @return EOK on success or an error code.
 */
static int preproc_process_ws(preproc_t *preproc, bool *rws)
{
	char *p;
	bool ws = false;

	/* Skip whitespace. */
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);

		if (p[0] != ' ' && p[0] != '\t')
			break;

		preproc_advance(preproc, 1);
		ws = true;
	}

	if (rws != NULL)
		*rws = ws;

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

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		return rc;

	if (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (p[0] != '\n') {
			(void)preproc_dprint_range(&preproc->cur->pos,
			    &preproc->cur->pos, stderr);
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

/** Process condition expression.
 *
 * @param preproc Preprocessor
 * @param rresult Place to store result
 * @return EOK on success or an error code.
 */
static int preproc_process_condition(preproc_t *preproc, bool *rresult)
{
	char *p;

	if (preproc_is_eof(preproc) || preproc_is_error(preproc))
		return EINVAL;

	p = preproc_chars(preproc);
	if (p[0] == '0') {
		preproc_advance(preproc, 1);
		*rresult = false;
		return EOK;
	}

	if (p[0] == '1') {
		preproc_advance(preproc, 1);
		*rresult = true;
		return EOK;
	}

	fprintf(stderr, "Invalid conditional expression.\n");
	return EINVAL;
}

/** Create preprocessor macro replacement list buffer.
 *
 * @param rbuf Place to store pointer to replacement list buffer.
 * @return @c EOK on success, @c ENOMEM if out of memory.
 */
static int preproc_rlist_buf_create(preproc_rlist_buf_t **rbuf)
{
	preproc_rlist_buf_t *buf;

	buf = calloc(1, sizeof(preproc_rlist_buf_t));
	if (buf == NULL)
		return ENOMEM;

	buf->buf_alloc_size = 32;
	buf->buf = malloc(buf->buf_alloc_size);
	if (buf->buf == NULL) {
		free(buf);
		return ENOMEM;
	}

	buf->buf_used = 0;
	*rbuf = buf;
	return EOK;
}

/** Destroy preprocessor macro replacement list buffer.
 *
 * @param buf Replacement list buffer
 */
static void preproc_rlist_buf_destroy(preproc_rlist_buf_t *buf)
{
	if (buf->buf != NULL)
		free(buf->buf);
	free(buf);
}

/** Append character to preprocessor macro replacement list buffer.
 *
 * @param buf Replacement list buffer
 */
static int preproc_rlist_buf_append(preproc_rlist_buf_t *buf, char c)
{
	char *newbuf;

	if (buf->buf_used >= buf->buf_alloc_size) {
		newbuf = realloc(buf->buf, buf->buf_alloc_size * 2);
		if (newbuf == NULL)
			return ENOMEM;

		buf->buf = newbuf;
		buf->buf_alloc_size = buf->buf_alloc_size * 2;
	}

	buf->buf[buf->buf_used++] = c;
	return EOK;
}

/** Process replacement list.
 *
 * @param preproc Preprocessor
 * @param rreplacement Place to store new replacement string.
 * @return EOK on success or an error code.
 */
static int preproc_process_replacement_list(preproc_t *preproc,
    char **rreplacement)
{
	char *p;
	preproc_rlist_buf_t *rlist = NULL;
	bool ws;
	int rc;

	rc = preproc_rlist_buf_create(&rlist);
	if (rc != EOK)
		goto error;

	/* Ignore whitespace at beginning of replacement list. */
	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		rc = preproc_process_ws(preproc, &ws);
		if (rc != EOK)
			goto error;

		while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
			p = preproc_chars(preproc);
			if (p[0] == ' ' || p[0] == '\t' || p[0] == '\n')
				break;

			if (ws) {
				rc = preproc_rlist_buf_append(rlist, ' ');
				if (rc != EOK)
					goto error;
				ws = false;
			}

			rc = preproc_rlist_buf_append(rlist, p[0]);
			if (rc != EOK)
				goto error;
			preproc_advance(preproc, 1);
		}

		if (preproc_is_eof(preproc) || preproc_is_error(preproc))
			break;

		p = preproc_chars(preproc);
		if (p[0] == '\n')
			break;
	}

	rc = preproc_rlist_buf_append(rlist, '\0');
	if (rc != EOK)
		goto error;

	*rreplacement = rlist->buf;
	rlist->buf = NULL;
	preproc_rlist_buf_destroy(rlist);
	return EOK;
error:
	if (rlist != NULL)
		preproc_rlist_buf_destroy(rlist);
	return rc;
}

/** Process invalid directive and print diagnostics.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code
 */
static int preproc_process_invalid_directive(preproc_t *preproc)
{
	src_pos_t spos;
	char *p;
	int rc;

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	spos = preproc->cur->pos;
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (!is_idcnt(p[0]))
			break;

		preproc_advance(preproc, 1);
	}

	(void)preproc_dprint_range(&spos, &preproc->cur->pos, stderr);
	(void)fprintf(stderr, ": Invalid preprocessor directive.\n");

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (p[0] == '\n')
			break;

		preproc_advance(preproc, 1);
	}

	if (preproc_is_error(preproc))
		return EIO;

	preproc->state = pps_line_begin;
	return EOK;
}

static int preproc_end_of_file_checks(preproc_t *preproc)
{
	preproc_condition_t *cond;

	cond = preproc_top_condition(preproc);
	if (cond != NULL) {
		(void)preproc_dprint_range(&cond->bpos, &cond->epos, stderr);
		(void)fprintf(stderr, ": Unterminated #if/#ifdef/#ifndef.\n");
		return EINVAL;
	}

	return EOK;
}

/** Look for an included file relative to the specified directory.
 *
 * @param preproc Preprocessor
 * @param dirname Directory relative to which to look for the file
 * @param file_name Include file name
 * @return EOK on success or an error code
 */
static int preproc_include_from_dir(preproc_t *preproc, const char *dirname,
    const char *file_name)
{
	FILE *file = NULL;
	file_input_t *finput = NULL;
	char *hdrname = NULL;
	int rc;

	hdrname = pathname_compose(dirname, file_name);
	if (hdrname == NULL) {
		rc = ENOMEM;
		goto error;
	}

	file = fopen(hdrname, "rt");
	if (file == NULL) {
		rc = ENOENT;
		goto error;
	}

	rc = file_input_create(file, file_name, &finput);
	if (rc != EOK)
		goto error;

	rc = preproc_push_input(preproc, file_name, file, finput,
	    &lexer_file_input, (void *)finput);
	if (rc != EOK)
		goto error;

	free(hdrname);
	return EOK;
error:
	if (hdrname != NULL)
		free(hdrname);
	if (finput != NULL)
		file_input_destroy(finput);
	if (file != NULL)
		(void)fclose(file);

	return rc;
}

/** Skip input until end of line.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_skip_to_end_of_line(preproc_t *preproc)
{
	char *p;
	char c;

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		c = p[0];
		preproc_advance(preproc, 1);

		if (c == '\n')
			break;
	}

	if (preproc_is_error(preproc))
		return EIO;

	return EOK;
}

/** Dump input until end of line.
 *
 * @param preproc Preprocessor
 * @param outf Output file
 * @return EOK on success or an error code.
 */
static int preproc_dump_to_end_of_line(preproc_t *preproc, FILE *outf)
{
	char *p;
	char c;
	int rv;

	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		c = p[0];
		preproc_advance(preproc, 1);
		rv = fputc(c, outf);
		if (rv < 0)
			return EIO;

		if (c == '\n')
			break;
	}

	if (preproc_is_error(preproc))
		return EIO;

	return EOK;
}

/** Include a file.
 *
 * @param preproc Preprocessor
 * @param inctype Include type (angle brackets or quotes)
 * @param file_name Include file name
 * @return EOK on success or an error code
 */
static int preproc_include(preproc_t *preproc, preproc_include_type_t inctype,
    const char *file_name)
{
	char *dirname = NULL;
	const char *incldir;
	int rc;

	/* For #include "header" look in the source C file's directory. */
	if (inctype == pit_quoted) {
		dirname = pathname_get_dirname(preproc->cur->in_fname);
		if (dirname == NULL) {
			rc = ENOMEM;
			goto error;
		}

		rc = preproc_include_from_dir(preproc, dirname, file_name);
		if (rc != EOK && rc != ENOENT)
			goto error;

		free(dirname);
		dirname = NULL;
		return EOK;
	}

	if (preproc->incldir != NULL)
		incldir = preproc->incldir;
	else
		incldir = "./lib/clib/include";

	/* Always look in the compiler's include directory list. */
	rc = preproc_include_from_dir(preproc, incldir, file_name);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	if (dirname != NULL)
		free(dirname);

	return rc;
}

/** Process macro name.
 *
 * @param preproc Preprocessor
 * @param rname Place to store pointer to newly allocated macro name.
 * @return EOK on success or an error code.
 */
static int preproc_process_macro_name(preproc_t *preproc, char **rname)
{
	char *p;
	char *macro_name;
	size_t buf_pos;
	int rc;

	macro_name = calloc(preproc_macro_name_buf_size, 1);
	if (macro_name == NULL)
		return ENOMEM;

	if (preproc_is_eof(preproc) || preproc_is_error(preproc)) {
		rc = EINVAL;
		goto error;
	}

	buf_pos = 0;
	while (!preproc_is_eof(preproc) && !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		if (!is_idcnt(p[0]))
			break;

		if (buf_pos >= preproc_macro_name_buf_size - 1) {
			(void)fprintf(stderr, "Macro name too long.\n");
			rc = EINVAL;
			goto error;
		}

		macro_name[buf_pos++] = p[0];
		preproc_advance(preproc, 1);
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	macro_name[buf_pos] = '\0';
	*rname = macro_name;
	return EOK;
error:
	free(macro_name);
	return rc;
}

/** Process define directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_define(preproc_t *preproc)
{
	char *macro_name = NULL;
	char *replacement = NULL;
	preproc_macro_t *old_macro;
	src_pos_t spos;
	int rc;

	spos = preproc->cur->pos;
	preproc_advance(preproc, 6);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_macro_name(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	rc = preproc_process_macro_name(preproc, &macro_name);
	if (rc != EOK)
		goto error;

	rc = preproc_process_replacement_list(preproc, &replacement);
	if (rc != EOK)
		goto error;

	/* Macro already exists? */
	old_macro = preproc_macro_find(preproc, macro_name);
	if (old_macro != NULL) {
		/* If the replacement list is different, generate error. */
		if (strcmp(replacement, old_macro->replacement) != 0) {
			(void)preproc_dprint_range(&spos,
			    &preproc->cur->pos, stderr);
			(void)fprintf(stderr, ": Macro '%s' redefined "
			    "with different replacement list.\n", macro_name);
			rc = EINVAL;
			goto error;
		}
	}

	if (old_macro == NULL) {
		rc = preproc_macro_add(preproc, macro_name, replacement);
		if (rc != EOK)
			goto error;
	}

	preproc->state = pps_line_begin;
	free(macro_name);
	free(replacement);
	return EOK;
error:
	if (macro_name != NULL)
		free(macro_name);
	if (replacement != NULL)
		free(replacement);
	return rc;
}

/** Process elif directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_elif(preproc_t *preproc)
{
	preproc_condition_t *old_cond;
	preproc_condition_t *cond;
	src_pos_t bpos;
	bool cond_result = false;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 4);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;
	} else {
		rc = preproc_process_ws(preproc, NULL);
		if (rc != EOK)
			goto error;

		if (preproc_is_eof(preproc)) {
			preproc_error_condition(preproc);
			rc = EINVAL;
			goto error;
		}

		if (preproc_is_error(preproc)) {
			rc = EIO;
			goto error;
		}

		rc = preproc_process_condition(preproc, &cond_result);
		if (rc != EOK)
			goto error;

		rc = preproc_process_ws_eol(preproc);
		if (rc != EOK)
			goto error;
	}

	old_cond = preproc_top_condition(preproc);
	if (old_cond == NULL) {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": Unmatched #elif.\n");
		rc = EINVAL;
		goto error;
	}

	if (old_cond->has_else) {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": #elif after #else for condition "
		    "starting at ");
		(void)preproc_dprint_range(&old_cond->bpos, &old_cond->epos,
		    stderr);
		(void)fprintf(stderr, ".\n");
		rc = EINVAL;
		goto error;
	}

	preproc_pop_condition(preproc);

	cond = preproc_push_condition(preproc, &bpos, &preproc->cur->pos,
	    preproc->skipping);
	if (cond == NULL) {
		rc = ENOMEM;
		goto error;
	}

	if (!preproc->skipping)
		preproc->skipping = !cond_result;

	preproc->state = pps_line_begin;
	return EOK;
error:
	return rc;
}

/** Process else directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_else(preproc_t *preproc)
{
	preproc_condition_t *cond;
	src_pos_t bpos;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 4);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;
	} else {
		rc = preproc_process_ws_eol(preproc);
		if (rc != EOK)
			goto error;
	}

	cond = preproc_top_condition(preproc);
	if (cond == NULL) {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": Unmatched #else.\n");
		rc = EINVAL;
		goto error;
	}

	if (cond->has_else) {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": Second #else for condition "
		    "starting at ");
		(void)preproc_dprint_range(&cond->bpos, &cond->epos, stderr);
		(void)fprintf(stderr, ".\n");
		rc = EINVAL;
		goto error;
	}

	cond->has_else = true;
	cond->else_bpos = bpos;
	cond->else_epos = preproc->cur->pos;

	preproc->skipping = !preproc->skipping;
	preproc->state = pps_line_begin;
	return EOK;
error:
	return rc;
}

/** Process endif directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_endif(preproc_t *preproc)
{
	preproc_condition_t *cond;
	src_pos_t bpos;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 5);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;
	} else {
		rc = preproc_process_ws_eol(preproc);
		if (rc != EOK)
			goto error;
	}

	cond = preproc_top_condition(preproc);
	if (cond == NULL) {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": Unmatched #endif.\n");
		rc = EINVAL;
		goto error;
	}

	preproc->skipping = cond->was_skipping;
	preproc_pop_condition(preproc);
	preproc->state = pps_line_begin;
	return EOK;
error:
	return rc;
}

/** Process error directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_error(preproc_t *preproc)
{
	src_pos_t bpos;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 5);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;
	} else {
		(void)preproc_dprint_range(&bpos, &preproc->cur->pos, stderr);
		(void)fprintf(stderr, ": #error");

		(void)preproc_dump_to_end_of_line(preproc, stderr);

		rc = EINVAL;
		goto error;
	}

	preproc->state = pps_line_begin;
	return EOK;
error:
	return rc;
}

/** Process if directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_if(preproc_t *preproc)
{
	preproc_condition_t *cond;
	src_pos_t bpos;
	bool cond_result;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 2);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		cond = preproc_push_condition(preproc, &bpos,
		    &preproc->cur->pos, preproc->skipping);
		if (cond == NULL) {
			rc = ENOMEM;
			goto error;
		}

		preproc->state = pps_line_begin;
		return EOK;
	}

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_condition(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	rc = preproc_process_condition(preproc, &cond_result);
	if (rc != EOK)
		goto error;

	rc = preproc_process_ws_eol(preproc);
	if (rc != EOK)
		goto error;

	cond = preproc_push_condition(preproc, &bpos, &preproc->cur->pos,
	    preproc->skipping);
	if (cond == NULL) {
		rc = ENOMEM;
		goto error;
	}

	preproc->skipping = !cond_result;

	preproc->state = pps_line_begin;
	return EOK;
error:
	return rc;
}

/** Process ifdef directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_ifdef(preproc_t *preproc)
{
	char *macro_name = NULL;
	preproc_condition_t *cond;
	preproc_macro_t *macro;
	src_pos_t bpos;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 5);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		cond = preproc_push_condition(preproc, &bpos,
		    &preproc->cur->pos, preproc->skipping);
		if (cond == NULL) {
			rc = ENOMEM;
			goto error;
		}

		preproc->state = pps_line_begin;
		return EOK;
	}

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_macro_name(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	rc = preproc_process_macro_name(preproc, &macro_name);
	if (rc != EOK)
		goto error;

	rc = preproc_process_ws_eol(preproc);
	if (rc != EOK)
		goto error;

	cond = preproc_push_condition(preproc, &bpos, &preproc->cur->pos,
	    preproc->skipping);
	if (cond == NULL) {
		rc = ENOMEM;
		goto error;
	}

	macro = preproc_macro_find(preproc, macro_name);
	if (macro == NULL)
		preproc->skipping = true;

	free(macro_name);
	preproc->state = pps_line_begin;
	return EOK;
error:
	if (macro_name != NULL)
		free(macro_name);
	return rc;
}

/** Process ifndef directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_ifndef(preproc_t *preproc)
{
	char *macro_name = NULL;
	preproc_condition_t *cond;
	preproc_macro_t *macro;
	src_pos_t bpos;
	int rc;

	bpos = preproc->cur->pos;
	preproc_advance(preproc, 6);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		cond = preproc_push_condition(preproc, &bpos,
		    &preproc->cur->pos, preproc->skipping);
		if (cond == NULL) {
			rc = ENOMEM;
			goto error;
		}

		preproc->state = pps_line_begin;
		return EOK;
	}

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_macro_name(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	rc = preproc_process_macro_name(preproc, &macro_name);
	if (rc != EOK)
		goto error;

	rc = preproc_process_ws_eol(preproc);
	if (rc != EOK)
		goto error;

	cond = preproc_push_condition(preproc, &bpos, &preproc->cur->pos,
	    preproc->skipping);
	if (cond == NULL) {
		rc = ENOMEM;
		goto error;
	}

	macro = preproc_macro_find(preproc, macro_name);
	if (macro != NULL)
		preproc->skipping = true;

	free(macro_name);
	preproc->state = pps_line_begin;
	return EOK;
error:
	if (macro_name != NULL)
		free(macro_name);
	return rc;
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

	preproc_advance(preproc, 7);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	fname = calloc(preproc_fname_buf_size, 1);
	if (fname == NULL)
		return ENOMEM;

	rc = preproc_process_ws(preproc, NULL);
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
			(void)fprintf(stderr, "Include filename too long.\n");
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
		(void)preproc_dprint_range(&preproc->cur->pos,
		    &preproc->cur->pos, stderr);
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

	rc = preproc_include(preproc, delim == '>' ? pit_angled : pit_quoted,
	    fname);
	if (rc != EOK)
		goto error;

	free(fname);
	preproc->state = pps_line_begin;
	return EOK;
error:
	free(fname);
	return rc;
}

/** Process pragma directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_pragma(preproc_t *preproc)
{
	int rc;

	preproc_advance(preproc, 6);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	preproc->state = pps_copy_pragma;
	preproc->pragma_pos = 0;
	return EOK;
}

/** Process undef directive.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_undef(preproc_t *preproc)
{
	char *macro_name = NULL;
	preproc_macro_t *macro;
	int rc;

	preproc_advance(preproc, 5);

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		goto error;

	if (preproc_is_eof(preproc)) {
		preproc_error_macro_name(preproc);
		rc = EINVAL;
		goto error;
	}

	if (preproc_is_error(preproc)) {
		rc = EIO;
		goto error;
	}

	rc = preproc_process_macro_name(preproc, &macro_name);
	if (rc != EOK)
		goto error;

	rc = preproc_process_ws_eol(preproc);
	if (rc != EOK)
		goto error;

	macro = preproc_macro_find(preproc, macro_name);
	if (macro != NULL) {
		preproc_macro_remove(macro);
	}

	free(macro_name);
	preproc->state = pps_line_begin;
	return EOK;
error:
	if (macro_name != NULL)
		free(macro_name);
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
	rc = preproc_process_ws(preproc, NULL);
	if (rc != EOK)
		return rc;

	p = preproc_chars(preproc);

	switch (p[0]) {
	case 'd':
		if (p[1] == 'e' && p[2] == 'f' && p[3] == 'i' &&
		    p[4] == 'n' && p[5] == 'e' && !is_idcnt(p[6])) {
			return preproc_process_define(preproc);
		}
		break;
	case 'e':
		if (p[1] == 'l' && p[2] == 'i' && p[3] == 'f' &&
		    !is_idcnt(p[4])) {
			return preproc_process_elif(preproc);
		}
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
		    !is_idcnt(p[4])) {
			return preproc_process_else(preproc);
		}
		if (p[1] == 'n' && p[2] == 'd' && p[3] == 'i' &&
		    p[4] == 'f' && !is_idcnt(p[5])) {
			return preproc_process_endif(preproc);
		}
		if (p[1] == 'r' && p[2] == 'r' && p[3] == 'o' &&
		    p[4] == 'r' && !is_idcnt(p[5])) {
			return preproc_process_error(preproc);
		}
		break;
	case 'i':
		if (p[1] == 'f' && !is_idcnt(p[2])) {
			return preproc_process_if(preproc);
		}
		if (p[1] == 'f' && p[2] == 'd' && p[3] == 'e' &&
		    p[4] == 'f' && !is_idcnt(p[5])) {
			return preproc_process_ifdef(preproc);
		}
		if (p[1] == 'f' && p[2] == 'n' && p[3] == 'd' &&
		    p[4] == 'e' && p[5] == 'f' && !is_idcnt(p[6])) {
			return preproc_process_ifndef(preproc);
		}
		if (p[1] == 'n' && p[2] == 'c' && p[3] == 'l' &&
		    p[4] == 'u' && p[5] == 'd' && p[6] == 'e' &&
		    !is_idcnt(p[7])) {
			return preproc_process_include(preproc);
		}
		break;
	case 'p':
		if (p[1] == 'r' && p[2] == 'a' && p[3] == 'g' &&
		    p[4] == 'm' && p[5] == 'a' && !is_idcnt(p[6])) {
			return preproc_process_pragma(preproc);
		}
		break;
	case 'u':
		if (p[1] == 'n' && p[2] == 'd' && p[3] == 'e' &&
		    p[4] == 'f' && !is_idcnt(p[5])) {
			return preproc_process_undef(preproc);
		}
		break;
	default:
		break;
	}

	return preproc_process_invalid_directive(preproc);
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

	if (preproc_is_eof(preproc)) {
		preproc->cur->out_eof = true;
		return EOK;
	}

	p = preproc_chars(preproc);
	if (p[0] == '#') {
		/* Preprocessor directive. */
		rc = preproc_process_directive(preproc);
		if (rc != EOK)
			return rc;
	} else {
		/* Text line. */
		preproc->state = pps_text_line;
		preproc->expand_eol = false;
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
	char c;
	int rc;
	src_pos_t pos;

	if (preproc->skipping) {
		rc = preproc_skip_to_end_of_line(preproc);
		if (rc != EOK)
			return rc;

		preproc->state = pps_line_begin;
		return EOK;
	}

	while (!preproc_xbuf_is_eof(preproc) &&
	    !preproc_xbuf_is_error(preproc)) {
		p = preproc_xbuf_chars(preproc);
		c = p[0];
		pos = preproc->xposbuf[preproc->xbuf_pos];

		preproc_out_buf_insert(preproc, c, &pos);

		preproc_xbuf_advance(preproc, 1);
		if (c == '\n') {
			preproc->state = pps_line_begin;
			break;
		}

		if (!preproc_out_buf_avail(preproc))
			break;
	}

	if (preproc_xbuf_is_eof(preproc))
		preproc->state = pps_line_begin;

	if (preproc_xbuf_is_error(preproc))
		return EIO;

	return EOK;
}

/** Process in copy pragma state.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code.
 */
static int preproc_process_copy_pragma(preproc_t *preproc)
{
	const char *pragma = "#pragma";

	assert(!preproc->skipping);

	while (preproc->pragma_pos < 7) {
		preproc_out_buf_insert(preproc, pragma[preproc->pragma_pos],
		    &preproc->cur->pos);
		++preproc->pragma_pos;

		if (!preproc_out_buf_avail(preproc))
			break;
	}

	if (preproc->pragma_pos >= 7) {
		preproc->state = pps_text_line;
	}

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
	switch (preproc->state) {
	case pps_line_begin:
		return preproc_process_line_begin(preproc);
	case pps_text_line:
		return preproc_process_text_line(preproc);
	case pps_copy_pragma:
		return preproc_process_copy_pragma(preproc);
	}

	return EINVAL;
}

/** Do some expansion.
 *
 * This may or may not write data to the expansion buffer.
 *
 * @param preproc Preprocessor
 * @return EOK on success or an error code
 */
static int preproc_expand_process(preproc_t *preproc)
{
	char c;
	char *p;

	while (!preproc->expand_eol && !preproc_is_eof(preproc) &&
	    !preproc_is_error(preproc)) {
		p = preproc_chars(preproc);
		c = p[0];

		preproc_xbuf_insert(preproc, c, &preproc->cur->pos);

		preproc_advance(preproc, 1);
		if (c == '\n') {
			preproc->expand_eol = true;
			break;
		}

		if (!preproc_xbuf_avail(preproc))
			break;
	}

	if (preproc_is_error(preproc))
		return EIO;
	return EOK;
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

	while (preproc->out_buf_used == 0 && preproc->cur != NULL &&
	    !preproc_is_error(preproc)) {
		/* Reached end of file. */
		if (preproc->cur->out_eof) {
			rc = preproc_end_of_file_checks(preproc);
			if (rc != EOK)
				return rc;

			preproc_pop_input(preproc);
			if (preproc->cur == NULL)
				break;
		}

		rc = preproc_process(preproc);
		if (rc != EOK)
			return rc;
	}

	if (preproc->cur != NULL && preproc_is_error(preproc))
		return EIO;

	if (bsize < preproc->out_buf_used)
		nbytes = bsize;
	else
		nbytes = preproc->out_buf_used;

	memcpy(buf, preproc->out_buf, nbytes);
	preproc->out_buf_used -= nbytes;

	*bpos = preproc->out_posbuf[0];
	*nread = nbytes;

	memcpy(preproc->out_buf, preproc->out_buf + nbytes,
	    preproc->out_buf_used);
	memcpy(preproc->out_posbuf, preproc->out_posbuf + nbytes,
	    preproc->out_buf_used * sizeof(src_pos_t));

	return EOK;
}
