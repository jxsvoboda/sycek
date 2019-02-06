/*
 * Copyright 2019 Jiri Svoboda
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
 * Lexer (lexical analyzer)
 */

#include <assert.h>
#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Create lexer.
 *
 * @param ops Input ops
 * @param arg Argument to input
 * @param rlexer Place to store pointer to new lexer
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int lexer_create(lexer_input_ops_t *ops, void *arg, lexer_t **rlexer)
{
	lexer_t *lexer;

	lexer = calloc(1, sizeof(lexer_t));
	if (lexer == NULL)
		return ENOMEM;

	lexer->input_ops = ops;
	lexer->input_arg = arg;
	*rlexer = lexer;
	return EOK;
}

/** Destroy lexer.
 *
 * @param lexer Lexer
 */
void lexer_destroy(lexer_t *lexer)
{
	if (lexer == NULL)
		return;

	free(lexer);
}

/** Determine if character is a letter (C language)
 *
 * @param c Character
 *
 * @return @c true if c is a letter (C language), @c false otherwise
 */
static bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Determine if character is a number (C language)
 *
 * @param c Character
 *
 * @return @c true if @a c is a number (C language), @c false otherwise
 */
static bool is_num(char c)
{
	return (c >= '0' && c <= '9');
}

/** Determine if character is alphanumeric (C language)
 *
 * @param c Character
 *
 * @return @c true if @a c is alphanumeric (C language), @c false otherwise
 */
static bool is_alnum(char c)
{
	return is_alpha(c) || is_num(c);
}

/** Determine if character is an octal digit
 *
 * @param c Character
 *
 * @return @c true if @a c is a octal digit, @c false otherwise
 */
static bool is_octdigit(char c)
{
	return c >= '0' && c <= '7';
}

/** Determine if character is a hexadecimal digit
 *
 * @param c Character
 *
 * @return @c true if @a c is a hexadecimal digit, @c false otherwise
 */
static bool is_hexdigit(char c)
{
	return is_num(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/** Determine if character is a digit in the specified base
 *
 * @param c Character
 * @param b Base (8, 10 or 16)
 *
 * @return @c true if @a c is a hexadecimal digit, @c false otherwise
 */
static bool is_digit(char c, int base)
{
	switch (base) {
	case 8:
		return is_octdigit(c);
	case 10:
		return is_num(c);
	case 16:
		return is_hexdigit(c);
	default:
		assert(false);
		return false;
	}
}

/** Determine if character can begin a C identifier
 *
 * @param c Character
 *
 * @return @c true if @a c can begin a C identifier, @c false otherwise
 */
static bool is_idbegin(char c)
{
	return is_alpha(c) || (c == '_');
}

/** Determine if character can continue a C identifier
 *
 * @param c Character
 *
 * @return @c true if @a c can continue a C identifier, @c false otherwise
 */
static bool is_idcnt(char c)
{
	return is_alnum(c) || (c == '_');
}

/** Determine if character is printable.
 *
 * A character that is part of a multibyte sequence is not printable.
 * @note This function assumes that the input is ASCII/UTF-8
 * @param c Character
 * @return @c true iff the character is printable
 */
static bool is_print(char c)
{
	uint8_t b;

	b = (uint8_t) c;
	return (b >= 32) && (b < 127);
}

/** Determine if character is a forbidden control character.
 *
 * This can only determine basic ASCII control characters.
 * Only allowed control characters are Tab, Line Feed (a.k.a. newline).
 *
 * @return @c true iff the character is a forbidden control character
 */
static bool is_bad_ctrl(char c)
{
	uint8_t b;

	b = (uint8_t) c;

	if (b < 32 && b != '\t' && b != '\n')
		return true;
	if (b == 127)
		return true;

	return false;
}

/** Get valid pointer to characters in input buffer.
 *
 * Returns a pointer into the input buffer, ensuring it contains
 * at least lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @return Pointer to characters in input buffer.
 */
static char *lexer_chars(lexer_t *lexer)
{
	int rc;
	size_t nread;
	src_pos_t rpos;

	if (!lexer->in_eof && lexer->buf_used - lexer->buf_pos <
	    lexer_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(lexer->buf, lexer->buf + lexer->buf_pos,
		    lexer->buf_used - lexer->buf_pos);
		lexer->buf_used -= lexer->buf_pos;
		lexer->buf_pos = 0;
		/* XX Advance lexer->buf_bpos */

		rc = lexer->input_ops->read(lexer->input_arg, lexer->buf +
		    lexer->buf_used, lexer_buf_size - lexer->buf_used,
		    &nread, &rpos);
		if (rc != EOK) {
			printf("read error\n");
		}
		if (nread < lexer_buf_size - lexer->buf_used)
			lexer->in_eof = true;
		if (lexer->buf_used == 0) {
			lexer->buf_bpos = rpos;
			lexer->pos = rpos;
		}
		lexer->buf_used += nread;
		if (lexer->buf_used < lexer_buf_size)
			lexer->buf[lexer->buf_used] = '\0';
	}

	assert(lexer->buf_pos < lexer_buf_size);
	return lexer->buf + lexer->buf_pos;
}

/** Get current lexer position in source code.
 *
 * @param lexer Lexer
 * @param pos Place to store position
 */
static void lexer_get_pos(lexer_t *lexer, src_pos_t *pos)
{
	*pos = lexer->pos;
}

/** Advance lexer read position.
 *
 * Advance read position by a certain amount of characters. Since all
 * input characters must be part of a token, the characters are added
 * to token @a tok.
 *
 * @param lexer Lexer
 * @param nchars Number of characters to advance
 * @param tok Token to which the characters should
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_advance(lexer_t *lexer, size_t nchars, lexer_tok_t *tok)
{
	char *p;

	while (nchars > 0) {
		tok->text = realloc(tok->text, tok->text_size + 2);
		if (tok->text == NULL)
			return ENOMEM;

		p = lexer_chars(lexer);
		tok->text[tok->text_size] = p[0];
		tok->text[tok->text_size + 1] = '\0';
		tok->text_size++;
		++lexer->buf_pos;
		assert(lexer->buf_pos < lexer_buf_size);
		src_pos_fwd_char(&lexer->pos, p[0]);
		--nchars;
	}

	return EOK;
}

/** Lex whitespace.
 *
 * @param lexer Lexer
 * @param ltt Token type (one of ltt_space, ltt_tab, ltt_newline, ltt_elbspace)
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_whitespace(lexer_t *lexer, lexer_toktype_t ltt,
    lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ltt;
	return lexer_advance(lexer, 1, tok);
}

/** Lex comment open.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_copen(lexer_t *lexer, lexer_tok_t *tok)
{
	int rc;

	lexer_get_pos(lexer, &tok->bpos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ltt_copen;
	lexer->state = ls_comment;
	return EOK;
}

/** Lex documentation comment open.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_dcopen(lexer_t *lexer, lexer_tok_t *tok)
{
	int rc;

	lexer_get_pos(lexer, &tok->bpos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ltt_dcopen;
	lexer->state = ls_comment;
	return EOK;
}

/** Lex comment close.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_cclose(lexer_t *lexer, lexer_tok_t *tok)
{
	int rc;

	lexer_get_pos(lexer, &tok->bpos);

	/* Skip trailing '*' */
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	/* Final '/' */
	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ltt_cclose;
	lexer->state = ls_normal;
	return EOK;
}

/** Lex comment text.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_ctext(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);

	p = lexer_chars(lexer);
	while (p[0] != ' ' && p[0] != '\t' && p[0] != '\n' &&
	    (p[0] != '*' || p[1] != '/')) {
		lexer_get_pos(lexer, &tok->epos);

		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
	}

	tok->ttype = ltt_ctext;
	return EOK;
}

/** Lex double-slash comment.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_dscomment(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	p = lexer_chars(lexer);
	while (p[0] != '\0' && (p[1] != '\n' || p[0] == '\\')) {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
	}

	lexer_get_pos(lexer, &tok->epos);

	if (p[0] != '\0') {
		/* Skip trailing newline */
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	}

	tok->ttype = ltt_dscomment;
	return EOK;
}

/** Lex preprocessor line.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_preproc(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);

	p = lexer_chars(lexer);

	/*
	 * Preprocessor frament ends with newline, except for
	 * backslash-newline
	 */
	while (p[0] != '\0' && (p[1] != '\n' || p[0] == '\\')) {
		if (p[0] == '/' && p[1] == '*') {
			/* Comment inside prerocessor line */
			rc = lexer_advance(lexer, 2, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
			while (p[0] != '\0' && (p[0] != '*' || p[1] != '/')) {
				rc = lexer_advance(lexer, 1, tok);
				if (rc != EOK) {
					lexer_free_tok(tok);
					return rc;
				}

				p = lexer_chars(lexer);
			}
		}

		if (p[0] != '\0') {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}
	}

	if (p[0] != '\0') {
		lexer_get_pos(lexer, &tok->epos);
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	}

	tok->ttype = ltt_preproc;
	return EOK;
}

/** Lex single-character token.
 *
 * @param lexer Lexer
 * @param ttype Token type
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_onechar(lexer_t *lexer, lexer_toktype_t ttype,
    lexer_tok_t *tok)
{
	char *p;

	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->text = malloc(2);
	p = lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	tok->text[0] = p[0];
	tok->text[1] = '\0';
	tok->ttype = ttype;
	return lexer_advance(lexer, 1, tok);
}

/** Lex keyword.
 *
 * @param lexer Lexer
 * @param ttype Token type
 * @param nchars Number of characters in the keyword
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_keyword(lexer_t *lexer, lexer_toktype_t ttype,
    size_t nchars, lexer_tok_t *tok)
{
	char *p;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);

	tok->text = malloc(nchars + 1);
	p = lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	memcpy(tok->text, p, nchars);
	tok->text[nchars] = '\0';

	rc = lexer_advance(lexer, nchars - 1, tok);
	if (rc != EOK)
		return rc;
	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ttype;
	return EOK;
}

/** Lex identifier.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_ident(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);
	p = lexer_chars(lexer);
	while (is_idcnt(p[1])) {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
	}

	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ltt_ident;
	return EOK;
}

/** Lex number.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_number(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	int rc;
	bool floating;
	int base;
	char exp_marker;
	char exp_cmarker;

	lexer_get_pos(lexer, &tok->bpos);
	p = lexer_chars(lexer);

	floating = false;

	/* Integer part */
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		/* Hexadecimal constant */
		base = 16;
		exp_marker = 'p';
		exp_cmarker = 'P';

		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
		while (is_hexdigit(p[1])) {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}
	} else if (is_num(p[0])) {
		base = (p[0] == '0' && is_num(p[1])) ? 8 : 10;
		exp_marker = 'e';
		exp_cmarker = 'E';

		/* Octal or decimal constant */
		while (is_digit(p[1], base)) {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}
	} else {
		base = 10;
		exp_marker = 'e';
		exp_cmarker = 'E';

		/* Starting with '.' */
		goto dec_point;
	}

	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

dec_point:
	/* Check for fractional part */
	p = lexer_chars(lexer);
	if (p[0] == '.') {
		floating = true;

		while (is_digit(p[1], base)) {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}

		lexer_get_pos(lexer, &tok->epos);
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	}

	/* Check for exponent */
	p = lexer_chars(lexer);
	if (p[0] == exp_marker || p[0] == exp_cmarker) {
		floating = true;

		/* Exponent sign */
		if (p[1] == '+' || p[1] == '-') {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}

		/* Exponent digits */
		while (is_num(p[1])) {
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}

		/* Last exponent digit */
		lexer_get_pos(lexer, &tok->epos);
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	}

	/* Check for suffixes */
	p = lexer_chars(lexer);
	if (floating) {
		if (p[0] == 'f' || p[0] == 'F' || p[0] == 'l' || p[0] == 'L') {
			lexer_get_pos(lexer, &tok->epos);
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}
		}
	} else {
		/* XXX Not precise */
		while (p[0] == 'u' || p[0] == 'U' || p[0] == 'l' ||
		    p[0] == 'L') {
			lexer_get_pos(lexer, &tok->epos);
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
		}
	}

	tok->ttype = ltt_number;
	return EOK;
}

/** Lex character or string literal.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_charstr(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_toktype_t ltt;
	char *p;
	char delim;
	int rc;

	lexer_get_pos(lexer, &tok->bpos);

	p = lexer_chars(lexer);

	if (p[0] == 'u' && p[1] == '8') {
		rc = lexer_advance(lexer, 2, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	} else if (p[0] == 'L' || p[0] == 'u' || p[0] == 'U') {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}
	}

	p = lexer_chars(lexer);
	switch (p[0]) {
	case '\'':
		delim = '\'';
		ltt = ltt_charlit;
		break;
	case '"':
		delim = '"';
		ltt = ltt_strlit;
		break;
	default:
		assert(false);
		return EINVAL;
	}

	while (true) {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
		if (p[0] == '\0') {
			tok->ttype = ltt_invalid;
			return EOK;
		}

		if (p[0] == delim)
			break;

		if (p[0] == '\\') {
			/* Skip the next character */
			rc = lexer_advance(lexer, 1, tok);
			if (rc != EOK) {
				lexer_free_tok(tok);
				return rc;
			}

			p = lexer_chars(lexer);
			if (p[0] == '\0') {
				tok->ttype = ltt_invalid;
				return EOK;
			}
		}
	}

	lexer_get_pos(lexer, &tok->epos);
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ltt;
	return EOK;
}

/** Lex non-printable character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_nonprint(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);

	tok->ttype = ltt_invchar;
	return lexer_advance(lexer, 1, tok);
}

/** Lex invalid character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_invalid(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);

	tok->ttype = ltt_invalid;
	return lexer_advance(lexer, 1, tok);
}

/** Lex End of File.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_eof(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ltt_eof;
	return EOK;
}

/** Lex next token in normal state.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_get_tok_normal(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;

	memset(tok, 0, sizeof(lexer_tok_t));

	p = lexer_chars(lexer);
	if (p[0] == '\0')
		return lexer_eof(lexer, tok);

	switch (p[0]) {
	case '\t':
		return lexer_whitespace(lexer, ltt_tab, tok);
	case '\n':
		return lexer_whitespace(lexer, ltt_newline, tok);
	case ' ':
		return lexer_whitespace(lexer, ltt_space, tok);
	case '!':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_notequal, 2, tok);
		return lexer_onechar(lexer, ltt_lnot, tok);
	case '"':
		return lexer_charstr(lexer, tok);
	case '#':
		return lexer_preproc(lexer, tok);
	case '%':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_modulo_assign, 2, tok);
		return lexer_onechar(lexer, ltt_modulo, tok);
	case '&':
		if (p[1] == '&')
			return lexer_keyword(lexer, ltt_land, 2, tok);
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_band_assign, 2, tok);
		return lexer_onechar(lexer, ltt_amper, tok);
	case '\'':
		return lexer_charstr(lexer, tok);
	case '(':
		return lexer_onechar(lexer, ltt_lparen, tok);
	case ')':
		return lexer_onechar(lexer, ltt_rparen, tok);
	case '*':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_times_assign, 2, tok);
		return lexer_onechar(lexer, ltt_asterisk, tok);
	case '+':
		if (p[1] == '+')
			return lexer_keyword(lexer, ltt_inc, 2, tok);
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_plus_assign, 2, tok);
		return lexer_onechar(lexer, ltt_plus, tok);
	case ',':
		return lexer_onechar(lexer, ltt_comma, tok);
	case '-':
		if (p[1] == '-')
			return lexer_keyword(lexer, ltt_dec, 2, tok);
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_minus_assign, 2, tok);
		if (p[1] == '>')
			return lexer_keyword(lexer, ltt_arrow, 2, tok);
		return lexer_onechar(lexer, ltt_minus, tok);
	case '.':
		if (p[1] == '.' && p[2] == '.')
			return lexer_keyword(lexer, ltt_ellipsis, 3, tok);
		if (is_num(p[1]))
			return lexer_number(lexer, tok);
		return lexer_onechar(lexer, ltt_period, tok);
	case '/':
		if (p[1] == '*' && p[2] == '*')
			return lexer_dcopen(lexer, tok);
		if (p[1] == '*')
			return lexer_copen(lexer, tok);
		if (p[1] == '/')
			return lexer_dscomment(lexer, tok);
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_divide_assign, 2, tok);
		return lexer_onechar(lexer, ltt_slash, tok);
	case ':':
		return lexer_onechar(lexer, ltt_colon, tok);
	case ';':
		return lexer_onechar(lexer, ltt_scolon, tok);
	case '<':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_lteq, 2, tok);
		if (p[1] == '<') {
			if (p[2] == '=') {
				return lexer_keyword(lexer, ltt_shl_assign,
				    3, tok);
			}

			return lexer_keyword(lexer, ltt_shl, 2, tok);
		}
		return lexer_onechar(lexer, ltt_less, tok);
	case '=':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_equal, 2, tok);
		return lexer_onechar(lexer, ltt_assign, tok);
	case '>':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_gteq, 2, tok);
		if (p[1] == '>') {
			if (p[2] == '=') {
				return lexer_keyword(lexer, ltt_shr_assign,
				    3, tok);
			}

			return lexer_keyword(lexer, ltt_shr, 2, tok);
		}
		return lexer_onechar(lexer, ltt_greater, tok);
	case '?':
		return lexer_onechar(lexer, ltt_qmark, tok);
	case 'L':
	case 'U':
		if (p[1] == '\'' || p[1] == '"')
			return lexer_charstr(lexer, tok);
		return lexer_ident(lexer, tok);
	case '~':
		return lexer_onechar(lexer, ltt_bnot, tok);
	case '^':
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_bxor_assign, 2, tok);
		return lexer_onechar(lexer, ltt_bxor, tok);
	case '{':
		return lexer_onechar(lexer, ltt_lbrace, tok);
	case '|':
		if (p[1] == '|')
			return lexer_keyword(lexer, ltt_lor, 2, tok);
		if (p[1] == '=')
			return lexer_keyword(lexer, ltt_bor_assign, 2, tok);
		return lexer_onechar(lexer, ltt_bor, tok);
	case '}':
		return lexer_onechar(lexer, ltt_rbrace, tok);
	case '[':
		return lexer_onechar(lexer, ltt_lbracket, tok);
	case '\\':
		if (p[1] == '\n')
			return lexer_whitespace(lexer, ltt_elbspace, tok);
		return lexer_invalid(lexer, tok);
	case ']':
		return lexer_onechar(lexer, ltt_rbracket, tok);
	case '_':
		if (p[1] == '_' && p[2] == 'a' && p[3] == 't' &&
		    p[4] == 't' && p[5] == 'r' && p[6] == 'i' &&
		    p[7] == 'b' && p[8] == 'u' && p[9] == 't' &&
		    p[10] == 'e' && p[11] == '_' && p[12] == '_' &&
		    !is_idcnt(p[13])) {
			return lexer_keyword(lexer, ltt_attribute, 13, tok);
		}
		if (p[1] == '_' && p[2] == 'i' && p[3] == 'n' &&
		    p[4] == 't' && p[5] == '1' && p[6] == '2' &&
		    p[7] == '8' && !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_int128, 8, tok);
		}
		if (p[1] == '_' && p[2] == 'r' && p[3] == 'e' &&
		    p[4] == 's' && p[5] == 't' && p[6] == 'r' &&
		    p[7] == 'i' && p[8] == 'c' && p[9] == 't' &&
		    p[10] == '_' && p[11] == '_' && !is_idcnt(p[12])) {
			return lexer_keyword(lexer, ltt_restrict_alt, 12, tok);
		}
		if (p[1] == 'A' && p[2] == 't' && p[3] == 'o' &&
		    p[4] == 'm' && p[5] == 'i' && p[6] == 'c' &&
		    !is_idcnt(p[7])) {
			return lexer_keyword(lexer, ltt_atomic, 7, tok);
		}
		return lexer_ident(lexer, tok);
	case 'a':
		if (p[1] == 's' && p[2] == 'm' && !is_idcnt(p[3])) {
			return lexer_keyword(lexer, ltt_asm, 3, tok);
		}
		if (p[1] == 'u' && p[2] == 't' && p[3] == 'o' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_auto, 4, tok);
		}
		return lexer_ident(lexer, tok);
	case 'b':
		if (p[1] == 'r' && p[2] == 'e' && p[3] == 'a' &&
		    p[4] == 'k' && !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_break, 5, tok);
		}
		return lexer_ident(lexer, tok);
	case 'c':
		if (p[1] == 'a' && p[2] == 's' && p[3] == 'e' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_case, 4, tok);
		}
		if (p[1] == 'h' && p[2] == 'a' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_char, 4, tok);
		}
		if (p[1] == 'o' && p[2] == 'n' && p[3] == 's' &&
		    p[4] == 't' && !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_const, 5, tok);
		}
		if (p[1] == 'o' && p[2] == 'n' && p[3] == 't' &&
		    p[4] == 'i' && p[5] == 'n' && p[6] == 'u' &&
		    p[7] == 'e' && !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_continue, 8, tok);
		}
		return lexer_ident(lexer, tok);
	case 'd':
		if (p[1] == 'o' && !is_idcnt(p[2])) {
			return lexer_keyword(lexer, ltt_do, 2, tok);
		}
		if (p[1] == 'o' && p[2] == 'u' && p[3] == 'b' && p[4] == 'l' &&
		    p[5] == 'e' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_double, 6, tok);
		}
		return lexer_ident(lexer, tok);
	case 'e':
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_else, 4, tok);
		}
		if (p[1] == 'n' && p[2] == 'u' && p[3] == 'm' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_enum, 4, tok);
		}
		if (p[1] == 'x' && p[2] == 't' && p[3] == 'e' && p[4] == 'r' &&
		    p[5] == 'n' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_extern, 6, tok);
		}
		return lexer_ident(lexer, tok);
	case 'f':
		if (p[1] == 'l' && p[2] == 'o' && p[3] == 'a' && p[4] == 't' &&
		    !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_float, 5, tok);
		}
		if (p[1] == 'o' && p[2] == 'r' && !is_idcnt(p[3])) {
			return lexer_keyword(lexer, ltt_for, 3, tok);
		}
		return lexer_ident(lexer, tok);
	case 'g':
		if (p[1] == 'o' && p[2] == 't' && p[3] == 'o' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_goto, 4, tok);
		}
		return lexer_ident(lexer, tok);
	case 'i':
		if (p[1] == 'f' && !is_idcnt(p[2]))
			return lexer_keyword(lexer, ltt_if, 2, tok);
		if (p[1] == 'n' && p[2] == 'l' && p[3] == 'i' && p[4] == 'n' &&
		    p[5] == 'e' && !is_idcnt(p[6]))
			return lexer_keyword(lexer, ltt_inline, 6, tok);
		if (p[1] == 'n' && p[2] == 't' && !is_idcnt(p[3]))
			return lexer_keyword(lexer, ltt_int, 3, tok);
		return lexer_ident(lexer, tok);
	case 'l':
		if (p[1] == 'o' && p[2] == 'n' && p[3] == 'g' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_long, 4, tok);
		}
		return lexer_ident(lexer, tok);
	case 'r':
		if (p[1] == 'e' && p[2] == 'g' && p[3] == 'i' && p[4] == 's' &&
		    p[5] == 't' && p[6] == 'e' && p[7] == 'r' &&
		    !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_register, 8, tok);
		}
		if (p[1] == 'e' && p[2] == 's' && p[3] == 't' && p[4] == 'r' &&
		    p[5] == 'i' && p[6] == 'c' && p[7] == 't' &&
		    !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_restrict, 8, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && p[3] == 'u' &&
		    p[4] == 'r' && p[5] == 'n' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_return, 6, tok);
		}
		return lexer_ident(lexer, tok);
	case 's':
		if (p[1] == 'h' && p[2] == 'o' && p[3] == 'r' && p[4] == 't' &&
		    !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_short, 5, tok);
		}
		if (p[1] == 'i' && p[2] == 'g' && p[3] == 'n' && p[4] == 'e' &&
		    p[5] == 'd' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_signed, 6, tok);
		}
		if (p[1] == 'i' && p[2] == 'z' && p[3] == 'e' && p[4] == 'o' &&
		    p[5] == 'f' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_sizeof, 6, tok);
		}
		if (p[1] == 't' && p[2] == 'a' && p[3] == 't' && p[4] == 'i' &&
		    p[5] == 'c' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_static, 6, tok);
		}
		if (p[1] == 't' && p[2] == 'r' && p[3] == 'u' && p[4] == 'c' &&
		    p[5] == 't' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_struct, 6, tok);
		}
		if (p[1] == 'w' && p[2] == 'i' && p[3] == 't' && p[4] == 'c' &&
		    p[5] == 'h' && !is_idcnt(p[6])) {
			return lexer_keyword(lexer, ltt_switch, 6, tok);
		}
		return lexer_ident(lexer, tok);
	case 't':
		if (p[1] == 'y' && p[2] == 'p' && p[3] == 'e' && p[4] == 'd' &&
		    p[5] == 'e' && p[6] == 'f' && !is_idcnt(p[7])) {
			return lexer_keyword(lexer, ltt_typedef, 7, tok);
		}
		return lexer_ident(lexer, tok);
	case 'u':
		if (p[1] == '\'' || p[1] == '"')
			return lexer_charstr(lexer, tok);
		if (p[1] == '8' && p[2] == '"')
			return lexer_charstr(lexer, tok);
		if (p[1] == 'n' && p[2] == 'i' && p[3] == 'o' && p[4] == 'n' &&
		    !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_union, 5, tok);
		}
		if (p[1] == 'n' && p[2] == 's' && p[3] == 'i' && p[4] == 'g' &&
		    p[5] == 'n' && p[6] == 'e' && p[7] == 'd' &&
		    !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_unsigned, 8, tok);
		}
		return lexer_ident(lexer, tok);
	case 'v':
		if (p[1] == 'o' && p[2] == 'i' && p[3] == 'd' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_void, 4, tok);
		}
		if (p[1] == 'o' && p[2] == 'l' && p[3] == 'a' && p[4] == 't' &&
		    p[5] == 'i' && p[6] == 'l' && p[7] == 'e' &&
		    !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_volatile, 8, tok);
		}
		return lexer_ident(lexer, tok);
	case 'w':
		if (p[1] == 'h' && p[2] == 'i' && p[3] == 'l' && p[4] == 'e' &&
		    !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_while, 5, tok);
		}
		return lexer_ident(lexer, tok);
	default:
		if (is_idbegin(p[0]))
			return lexer_ident(lexer, tok);
		if (is_num(p[0]))
			return lexer_number(lexer, tok);
		if (!is_print(p[0]))
			return lexer_nonprint(lexer, tok);
		return lexer_invalid(lexer, tok);
	}

	return EOK;
}

/** Lex next token in comment state.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_get_tok_comment(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	memset(tok, 0, sizeof(lexer_tok_t));

	p = lexer_chars(lexer);
	if (p[0] == '\0')
		return lexer_eof(lexer, tok);

	switch (p[0]) {
	case '\t':
		return lexer_whitespace(lexer, ltt_tab, tok);
	case '\n':
		return lexer_whitespace(lexer, ltt_newline, tok);
	case ' ':
		return lexer_whitespace(lexer, ltt_space, tok);
	case '*':
		if (p[1] == '/')
			return lexer_cclose(lexer, tok);
		return lexer_ctext(lexer, tok);
	default:
		return lexer_ctext(lexer, tok);
	}

	return EOK;
}

/** Determine if token consists only of allowed characters.
 *
 * @param tok Token to validate
 * @param offs Offset at which to start
 * @param invpos Place to store offset of the first invalid character
 * @return @c true if token only consist of valid characters
 */
bool lexer_tok_valid_chars(lexer_tok_t *tok, size_t offs, size_t *invpos)
{
	size_t pos;

	if (tok->text == NULL)
		return true;

	pos = offs;
	while (tok->text[pos] != '\0') {
		if (is_bad_ctrl(tok->text[pos])) {
			*invpos = pos;
			return false;
		}

		++pos;
	}

	return true;
}

/** Lex next token.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
int lexer_get_tok(lexer_t *lexer, lexer_tok_t *tok)
{
	int rc;

	switch (lexer->state) {
	case ls_normal:
		rc = lexer_get_tok_normal(lexer, tok);
		break;
	case ls_comment:
		rc = lexer_get_tok_comment(lexer, tok);
		break;
	}

	if (rc != EOK)
		return rc;

	return EOK;
}

/** Free token.
 *
 * Free/finalize token obtained via lex_get_tok().
 *
 * @param tok Token
 */
void lexer_free_tok(lexer_tok_t *tok)
{
	if (tok->text != NULL)
		free(tok->text);
	tok->text = NULL;
}

/** Return string representation of token type.
 *
 * @param ttype Token type
 * @return Constant string representing token type
 */
const char *lexer_str_ttype(lexer_toktype_t ttype)
{
	switch (ttype) {
	case ltt_space:
		return "space";
	case ltt_tab:
		return "tab";
	case ltt_newline:
		return "newline";
	case ltt_elbspace:
		return "\\";
	case ltt_copen:
		return "'/*'";
	case ltt_ctext:
		return "ctext";
	case ltt_ccont:
		return "ccont";
	case ltt_cclose:
		return "'*/'";
	case ltt_dcopen:
		return "'/**'";
	case ltt_dscomment:
		return "dscomment";
	case ltt_preproc:
		return "preproc";
	case ltt_lparen:
		return "'('";
	case ltt_rparen:
		return "')'";
	case ltt_lbrace:
		return "'{'";
	case ltt_rbrace:
		return "'}'";
	case ltt_comma:
		return "','";
	case ltt_colon:
		return "':'";
	case ltt_scolon:
		return "';'";
	case ltt_qmark:
		return "'?'";
	case ltt_period:
		return "'.'";
	case ltt_ellipsis:
		return "'...'";
	case ltt_arrow:
		return "'->'";
	case ltt_plus:
		return "'+'";
	case ltt_minus:
		return "'-'";
	case ltt_asterisk:
		return "'*'";
	case ltt_slash:
		return "'/'";
	case ltt_modulo:
		return "'%'";
	case ltt_inc:
		return "'++'";
	case ltt_dec:
		return "'--'";
	case ltt_shl:
		return "'<<'";
	case ltt_shr:
		return "'>>'";
	case ltt_amper:
		return "'&'";
	case ltt_bor:
		return "'|'";
	case ltt_bxor:
		return "'^'";
	case ltt_bnot:
		return "'~'";
	case ltt_land:
		return "'&&'";
	case ltt_lor:
		return "'||'";
	case ltt_lnot:
		return "'!'";
	case ltt_less:
		return "'<'";
	case ltt_greater:
		return "'>'";
	case ltt_equal:
		return "'=='";
	case ltt_lteq:
		return "'<='";
	case ltt_gteq:
		return "'>='";
	case ltt_notequal:
		return "'!='";
	case ltt_assign:
		return "'='";
	case ltt_plus_assign:
		return "'+='";
	case ltt_minus_assign:
		return "'-='";
	case ltt_times_assign:
		return "'*='";
	case ltt_divide_assign:
		return "'/='";
	case ltt_modulo_assign:
		return "'%='";
	case ltt_shl_assign:
		return "'<<='";
	case ltt_shr_assign:
		return "'>>='";
	case ltt_band_assign:
		return "'&='";
	case ltt_bor_assign:
		return "'|='";
	case ltt_bxor_assign:
		return "'^='";
	case ltt_lbracket:
		return "'['";
	case ltt_rbracket:
		return "']'";
	case ltt_atomic:
		return "_Atomic";
	case ltt_attribute:
		return "'__attribute__'";
	case ltt_asm:
		return "'asm'";
	case ltt_auto:
		return "'auto'";
	case ltt_break:
		return "'break'";
	case ltt_case:
		return "'case'";
	case ltt_char:
		return "'char'";
	case ltt_const:
		return "'const'";
	case ltt_continue:
		return "'continue'";
	case ltt_do:
		return "'do'";
	case ltt_double:
		return "'double'";
	case ltt_else:
		return "'else'";
	case ltt_enum:
		return "'enum'";
	case ltt_extern:
		return "'extern'";
	case ltt_float:
		return "'float'";
	case ltt_for:
		return "'for'";
	case ltt_goto:
		return "'goto'";
	case ltt_if:
		return "'if'";
	case ltt_inline:
		return "'inline'";
	case ltt_int:
		return "'int'";
	case ltt_int128:
		return "'__int128'";
	case ltt_long:
		return "'long'";
	case ltt_register:
		return "'register'";
	case ltt_restrict:
		return "'restrict'";
	case ltt_restrict_alt:
		return "'__restrict__'";
	case ltt_return:
		return "'return'";
	case ltt_signed:
		return "'signed'";
	case ltt_sizeof:
		return "'sizeof'";
	case ltt_short:
		return "'short'";
	case ltt_static:
		return "'static'";
	case ltt_struct:
		return "'struct'";
	case ltt_switch:
		return "'switch'";
	case ltt_typedef:
		return "'typedef'";
	case ltt_union:
		return "'union'";
	case ltt_unsigned:
		return "'unsigned'";
	case ltt_void:
		return "'void'";
	case ltt_volatile:
		return "'volatile'";
	case ltt_while:
		return "'while'";
	case ltt_ident:
		return "id";
	case ltt_number:
		return "num";
	case ltt_strlit:
		return "str";
	case ltt_charlit:
		return "char";
	case ltt_eof:
		return "eof";
	case ltt_invalid:
		return "invalid";
	case ltt_invchar:
		return "invchar";
	case ltt_error:
		return "error";
	}

	return EOK;
}

/** Print token type.
 *
 * @param ttype Token type
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int lexer_print_ttype(lexer_toktype_t ttype, FILE *f)
{
	if (fputs(lexer_str_ttype(ttype), f) < 0)
		return EIO;

	return EOK;
}

/** Print character, escaping special characters.
 *
 * @param c Character to print
 * @param f Output file
 * @return EOK on success, EIO on I/O error
 */
int lexer_dprint_char(char c, FILE *f)
{
	int rc;

	if (!is_print(c)) {
		rc = fprintf(f, "#%02x", c);
		if (rc < 0)
			return EIO;
	} else if (c == '#') {
		rc = fputc('#', f);
		if (rc == EOF)
			return EIO;
	} else {
		rc = fputc(c, f);
		if (rc == EOF)
			return EIO;
	}

	return EOK;
}

/** Print string, escaping special characters.
 *
 * @param str Strint to print
 * @param f Output file
 * @return EOK on success, EIO on I/O error
 */
static int lexer_dprint_str(const char *str, FILE *f)
{
	const char *cp;
	char c;
	int rc;

	cp = str;
	while (*cp != '\0') {
		c = *cp++;
		rc = lexer_dprint_char(c, f);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Print token structurally (for debugging) with specified range.
 *
 * @param tok Token
 * @param bpos Begin position
 * @param epos End position
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
static int lexer_dprint_tok_range(lexer_tok_t *tok, src_pos_t *bpos,
    src_pos_t *epos, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(bpos, epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ":%s", lexer_str_ttype(tok->ttype)) < 0)
		return EIO;

	switch (tok->ttype) {
	case ltt_ident:
	case ltt_number:
		if (fprintf(f, ":%s", tok->text) < 0)
			return EIO;
		break;
	case ltt_invalid:
	case ltt_invchar:
		if (fputc(':', f) == EOF)
			return EIO;
		rc = lexer_dprint_str(tok->text, f);
		if (rc != EOK)
			return EIO;
		break;
	default:
		break;
	}

	if (fprintf(f, ">") < 0)
		return EIO;

	return EOK;
}

/** Print token structurally (for debugging).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int lexer_dprint_tok(lexer_tok_t *tok, FILE *f)
{
	return lexer_dprint_tok_range(tok, &tok->bpos, &tok->epos, f);
}

/** Print token structurally (for debugging) pointing to a single character.
 *
 * @param tok Token
 * @param offs Offset of the character to print the range for
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int lexer_dprint_tok_chr(lexer_tok_t *tok, size_t offs, FILE *f)
{
	src_pos_t pos;
	size_t i;

	pos = tok->bpos;
	for (i = 0; i < offs; i++)
		src_pos_fwd_char(&pos, tok->text[i]);

	return lexer_dprint_tok_range(tok, &pos, &pos, f);
}

/** Print token (in original C form).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int lexer_print_tok(lexer_tok_t *tok, FILE *f)
{
	if (fprintf(f, "%s", tok->text) < 0)
		return EIO;
	return EOK;
}

/** Determine if token type is a comment token.
 *
 * @param ltt Token type
 * @return @c true if ltt is a comment token type
 */
bool lexer_is_comment(lexer_toktype_t ltt)
{
	return ltt == ltt_copen || ltt == ltt_ctext || ltt == ltt_ccont ||
	    ltt == ltt_cclose || ltt == ltt_dscomment || ltt == ltt_dcopen;
}

/** Determine if token type is a whitespace token.
 *
 * @param ltt Token type
 * @return @c true if ltt is a whitespace token type
 */
bool lexer_is_wspace(lexer_toktype_t ltt)
{
	return ltt == ltt_space || ltt == ltt_tab || ltt == ltt_newline;
}

/** Determine if token type is a reserved word token.
 *
 * @param ltt Token type
 * @return @c true if ltt is a reserved word token type
 */
bool lexer_is_resword(lexer_toktype_t ltt)
{
	return ltt >= ltt_resword_first &&
	    ltt <= ltt_resword_last;
}
