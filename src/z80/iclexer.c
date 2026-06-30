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
 * Z80 IC lexer (lexical analyzer)
 *
 * A lexical analyzer for the Z80 symbolic instruction code (IC).
 */

#include <assert.h>
#include <inttypes.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <z80/iclexer.h>

/** Create lexer.
 *
 * @param ops Input ops
 * @param arg Argument to input
 * @param rlexer Place to store pointer to new lexer
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int z80ic_lexer_create(lexer_input_ops_t *ops, void *arg, z80ic_lexer_t **rlexer)
{
	z80ic_lexer_t *lexer;

	lexer = calloc(1, sizeof(z80ic_lexer_t));
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
void z80ic_lexer_destroy(z80ic_lexer_t *lexer)
{
	if (lexer == NULL)
		return;

	free(lexer);
}

/** Determine if character is a letter (IR language)
 *
 * @param c Character
 *
 * @return @c true if c is a letter (IR language), @c false otherwise
 */
static bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Determine if character is a number (IR language)
 *
 * @param c Character
 *
 * @return @c true if @a c is a number (IR language), @c false otherwise
 */
static bool is_num(char c)
{
	return (c >= '0' && c <= '9');
}

/** Determine if character is alphanumeric (IR language)
 *
 * @param c Character
 *
 * @return @c true if @a c is alphanumeric (IR language), @c false otherwise
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

/** Determine if character can continue an IR identifier
 *
 * @param c Character
 *
 * @return @c true if @a c can continue an IR identifier, @c false otherwise
 */
static bool is_idcnt(char c)
{
	return is_alnum(c) || c == '_' || c == '@';
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
 * at least z80ic_lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @return Pointer to characters in input buffer.
 */
static char *z80ic_lexer_chars(z80ic_lexer_t *lexer)
{
	int rc;
	size_t nread;
	src_pos_t rpos;

	if (!lexer->in_eof && lexer->buf_used - lexer->buf_pos <
	    z80ic_lexer_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(lexer->buf, lexer->buf + lexer->buf_pos,
		    lexer->buf_used - lexer->buf_pos);
		lexer->buf_used -= lexer->buf_pos;
		lexer->buf_pos = 0;
		/* XX Advance lexer->buf_bpos */

		rc = lexer->input_ops->read(lexer->input_arg, lexer->buf +
		    lexer->buf_used, z80ic_lexer_buf_size - lexer->buf_used,
		    &nread, &rpos);
		if (rc != EOK) {
			lexer->in_error = true;
			nread = 0;
			rpos = lexer->pos;
		}
		if (nread < z80ic_lexer_buf_size - lexer->buf_used)
			lexer->in_eof = true;
		if (lexer->buf_used == 0) {
			lexer->buf_bpos = rpos;
			lexer->pos = rpos;
		}
		lexer->buf_used += nread;
		if (lexer->buf_used < z80ic_lexer_buf_size)
			lexer->buf[lexer->buf_used] = '\0';
	}

	assert(lexer->buf_pos < z80ic_lexer_buf_size);
	return lexer->buf + lexer->buf_pos;
}

/** Determine if lexer is at end of file.
 *
 * @param lexer Lexer
 * @return @c true iff there are no more characters available
 */
static bool z80ic_lexer_is_eof(z80ic_lexer_t *lexer)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = z80ic_lexer_chars(lexer);
	(void) lc;

	return lexer->buf_pos == lexer->buf_used;
}

/** Determine if lexer encountered an I/O error.
 *
 * @param lexer Lexer
 * @return @c true iff there was an I/O error
 */
static bool z80ic_lexer_is_error(z80ic_lexer_t *lexer)
{
	return lexer->in_error;
}

/** Get current lexer position in source code.
 *
 * @param lexer Lexer
 * @param pos Place to store position
 */
static void z80ic_lexer_get_pos(z80ic_lexer_t *lexer, src_pos_t *pos)
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
static int z80ic_lexer_advance(z80ic_lexer_t *lexer, size_t nchars, z80ic_lexer_tok_t *tok)
{
	char *p;
	char *ntext;

	while (nchars > 0) {
		ntext = realloc(tok->text, tok->text_size + 2);
		if (ntext == NULL)
			return ENOMEM;

		tok->text = ntext;

		p = z80ic_lexer_chars(lexer);
		tok->text[tok->text_size] = p[0];
		tok->text[tok->text_size + 1] = '\0';
		tok->text_size++;
		++lexer->buf_pos;
		assert(lexer->buf_pos <= z80ic_lexer_buf_size);
		src_pos_fwd_char(&lexer->pos, p[0]);
		--nchars;
	}

	return EOK;
}

/** Lex whitespace.
 *
 * @param lexer Lexer
 * @param itt Token type (one of ztt_space, ztt_tab, ztt_newline)
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_whitespace(z80ic_lexer_t *lexer, z80ic_lexer_toktype_t itt,
    z80ic_lexer_tok_t *tok)
{
	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = itt;
	return z80ic_lexer_advance(lexer, 1, tok);
}

/** Lex comment.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_comment(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	char *p;
	int rc;

	z80ic_lexer_get_pos(lexer, &tok->bpos);
	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
		return rc;
	}

	while (true) {
		rc = z80ic_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			z80ic_lexer_free_tok(tok);
			return rc;
		}

		p = z80ic_lexer_chars(lexer);
		if (p[0] == '\0') {
			tok->ttype = ztt_invalid;
			return EOK;
		}

		if (p[0] == '*' && p[1] == '/')
			break;
	}

	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
		return rc;
	}

	z80ic_lexer_get_pos(lexer, &tok->epos);

	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ztt_comment;
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
static int z80ic_lexer_onechar(z80ic_lexer_t *lexer, z80ic_lexer_toktype_t ttype,
    z80ic_lexer_tok_t *tok)
{
	char *p;

	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);
	tok->text = malloc(2);
	p = z80ic_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	tok->text[0] = p[0];
	tok->text[1] = '\0';
	tok->ttype = ttype;
	return z80ic_lexer_advance(lexer, 1, tok);
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
static int z80ic_lexer_keyword(z80ic_lexer_t *lexer, z80ic_lexer_toktype_t ttype,
    size_t nchars, z80ic_lexer_tok_t *tok)
{
	char *p;
	int rc;

	z80ic_lexer_get_pos(lexer, &tok->bpos);

	tok->text = malloc(nchars + 1);
	p = z80ic_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	memcpy(tok->text, p, nchars);
	tok->text[nchars] = '\0';

	rc = z80ic_lexer_advance(lexer, nchars - 1, tok);
	if (rc != EOK)
		return rc;
	z80ic_lexer_get_pos(lexer, &tok->epos);
	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
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
static int z80ic_lexer_ident(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	char *p;
	int rc;

	z80ic_lexer_get_pos(lexer, &tok->bpos);
	p = z80ic_lexer_chars(lexer);
	while (is_idcnt(p[1])) {
		rc = z80ic_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			z80ic_lexer_free_tok(tok);
			return rc;
		}

		p = z80ic_lexer_chars(lexer);
	}

	z80ic_lexer_get_pos(lexer, &tok->epos);
	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ztt_ident;
	return EOK;
}

/** Lex number.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_number(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	char *p;
	int rc;

	z80ic_lexer_get_pos(lexer, &tok->bpos);
	p = z80ic_lexer_chars(lexer);

	while (is_digit(p[1], 10)) {
		rc = z80ic_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			z80ic_lexer_free_tok(tok);
			return rc;
		}

		p = z80ic_lexer_chars(lexer);
	}

	z80ic_lexer_get_pos(lexer, &tok->epos);

	rc = z80ic_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		z80ic_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ztt_number;
	return EOK;
}

/** Lex non-printable character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_nonprint(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = ztt_invchar;
	return z80ic_lexer_advance(lexer, 1, tok);
}

/** Lex invalid character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_invalid(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = ztt_invalid;
	return z80ic_lexer_advance(lexer, 1, tok);
}

/** Lex End of File.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_eof(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ztt_eof;
	return EOK;
}

/** Lex Error.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int z80ic_lexer_error(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	z80ic_lexer_get_pos(lexer, &tok->bpos);
	z80ic_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ztt_error;
	return EOK;
}

/** Lex next token in.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using z80ic_lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
int z80ic_lexer_get_tok(z80ic_lexer_t *lexer, z80ic_lexer_tok_t *tok)
{
	char *p;

	memset(tok, 0, sizeof(z80ic_lexer_tok_t));

	p = z80ic_lexer_chars(lexer);

	/* End of file or null character */
	if (p[0] == '\0') {
		if (z80ic_lexer_is_error(lexer))
			return z80ic_lexer_error(lexer, tok);
		else if (z80ic_lexer_is_eof(lexer))
			return z80ic_lexer_eof(lexer, tok);
		else
			return z80ic_lexer_nonprint(lexer, tok);
	}

	switch (p[0]) {
	case '\t':
		return z80ic_lexer_whitespace(lexer, ztt_tab, tok);
	case '\n':
		return z80ic_lexer_whitespace(lexer, ztt_newline, tok);
	case ' ':
		return z80ic_lexer_whitespace(lexer, ztt_space, tok);
	case '%':
		return z80ic_lexer_ident(lexer, tok);
	case '(':
		return z80ic_lexer_onechar(lexer, ztt_lparen, tok);
	case ')':
		return z80ic_lexer_onechar(lexer, ztt_rparen, tok);
	case '+':
		return z80ic_lexer_onechar(lexer, ztt_plus, tok);
	case ',':
		return z80ic_lexer_onechar(lexer, ztt_comma, tok);
	case '-':
		return z80ic_lexer_onechar(lexer, ztt_minus, tok);
	case '.':
		return z80ic_lexer_onechar(lexer, ztt_period, tok);
	case '/':
		if (p[1] == '*')
			return z80ic_lexer_comment(lexer, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case ':':
		return z80ic_lexer_onechar(lexer, ztt_colon, tok);
	case ';':
		return z80ic_lexer_onechar(lexer, ztt_scolon, tok);
	case '@':
		return z80ic_lexer_ident(lexer, tok);
	case 'A':
		if (p[1] == 'F' && p[2] == '\'')
			return z80ic_lexer_keyword(lexer, ztt_AF_, 3, tok);
		if (p[1] == 'F' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_AF, 2, tok);
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_A, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'B':
		if (p[1] == 'C' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_BC, 2, tok);
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_B, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'C':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_C, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'D':
		if (p[1] == 'E' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_DE, 2, tok);
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_D, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'E':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_E, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'F':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_F, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'H':
		if (p[1] == 'L' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_HL, 2, tok);
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_H, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'I':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_I, 1, tok);
		if (p[1] == 'X' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_IX, 2, tok);
		if (p[1] == 'Y' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_IY, 2, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'L':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_L, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'M':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_M, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'N':
		if (p[1] == 'C' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_NC, 2, tok);
		if (p[1] == 'Z' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_NZ, 2, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'P':
		if (p[1] == 'E' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_PE, 2, tok);
		if (p[1] == 'O' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_PO, 2, tok);
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_P, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'R':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_R, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'S':
		if (p[1] == 'P' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_SP, 2, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'Z':
		if (!is_idcnt(p[1]))
			return z80ic_lexer_keyword(lexer, ztt_Z, 1, tok);
		return z80ic_lexer_invalid(lexer, tok);
	case 'a':
		if (p[1] == 'd' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_adc, 3, tok);
		}
		if (p[1] == 'd' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_add, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_and, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'b':
		if (p[1] == 'e' && p[2] == 'g' && p[3] == 'i' &&
		    p[4] == 'n' && !is_idcnt(p[5])) {
			return z80ic_lexer_keyword(lexer, ztt_begin, 5, tok);
		}
		if (p[1] == 'i' && p[2] == 't' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_bit, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'c':
		if (p[1] == 'a' && p[2] == 'l' && p[3] == 'l' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_call, 4, tok);
		}
		if (p[1] == 'c' && p[2] == 'f' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ccf, 3, tok);
		}
		if (p[1] == 'p' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_cp, 2, tok);
		if (p[1] == 'p' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_cpd, 3, tok);
		}
		if (p[1] == 'p' && p[2] == 'd' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_cpdr, 4, tok);
		}
		if (p[1] == 'p' && p[2] == 'i' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_cpi, 3, tok);
		}
		if (p[1] == 'p' && p[2] == 'i' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_cpir, 4, tok);
		}
		if (p[1] == 'p' && p[2] == 'l' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_cpl, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'd':
		if (p[1] == 'a' && p[2] == 'a' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_daa, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_dec, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 'f' && p[3] == 'b' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_defb, 4, tok);
		}
		if (p[1] == 'e' && p[2] == 'f' && p[3] == 'w' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_defw, 4, tok);
		}
		if (p[1] == 'e' && p[2] == 'f' && p[3] == 'd' &&
		    p[4] == 'w' && !is_idcnt(p[5])) {
			return z80ic_lexer_keyword(lexer, ztt_defdw, 5, tok);
		}
		if (p[1] == 'e' && p[2] == 'f' && p[3] == 'q' &&
		    p[4] == 'w' && !is_idcnt(p[5])) {
			return z80ic_lexer_keyword(lexer, ztt_defqw, 5, tok);
		}
		if (p[1] == 'i' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_di, 2, tok);
		if (p[1] == 'j' && p[2] == 'n' && p[3] == 'z' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_djnz, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'e':
		if (p[1] == 'i' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_ei, 2, tok);
		if (p[1] == 'n' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_end, 3, tok);
		}
		if (p[1] == 'x' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_ex, 2, tok);
		if (p[1] == 'x' && p[2] == 'x' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_exx, 3, tok);
		}
		if (p[1] == 'x' && p[2] == 't' && p[3] == 'e' &&
		    p[4] == 'r' && p[5] == 'n' && !is_idcnt(p[6])) {
			return z80ic_lexer_keyword(lexer, ztt_extern, 6, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'g':
		if (p[1] == 'l' && p[2] == 'o' && p[3] == 'b' &&
		    p[4] == 'a' && p[5] == 'l' && !is_idcnt(p[6])) {
			return z80ic_lexer_keyword(lexer, ztt_global, 6, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'h':
		if (p[1] == 'a' && p[2] == 'l' && p[3] == 't' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_halt, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'i':
		if (p[1] == 'm' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_im, 2, tok);
		if (p[1] == 'n' && !is_idcnt(p[2]))
			return z80ic_lexer_keyword(lexer, ztt_in, 2, tok);
		if (p[1] == 'n' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_inc, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ind, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'd' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_indr, 4, tok);
		}
		if (p[1] == 'n' && p[2] == 'i' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ini, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'i' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_inir, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'j':
		if (p[1] == 'p' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_jp, 2, tok);
		}
		if (p[1] == 'r' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_jr, 2, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'l':
		if (p[1] == 'd' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_ld, 2, tok);
		}
		if (p[1] == 'd' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ldd, 3, tok);
		}
		if (p[1] == 'd' && p[2] == 'd' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_lddr, 4, tok);
		}
		if (p[1] == 'd' && p[2] == 'i' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ldi, 3, tok);
		}
		if (p[1] == 'd' && p[2] == 'i' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_ldir, 4, tok);
		}
		if (p[1] == 'v' && p[2] == 'a' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_lvar, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'n':
		if (p[1] == 'e' && p[2] == 'g' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_neg, 3, tok);
		}
		if (p[1] == 'o' && p[2] == 'p' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_nop, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'o':
		if (p[1] == 'r' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_or, 2, tok);
		}
		if (p[1] == 't' && p[2] == 'd' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_otdr, 4, tok);
		}
		if (p[1] == 't' && p[2] == 'i' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_otir, 4, tok);
		}
		if (p[1] == 'u' && p[2] == 't' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_out, 3, tok);
		}
		if (p[1] == 'u' && p[2] == 't' && p[3] == 'd' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_outd, 4, tok);
		}
		if (p[1] == 'u' && p[2] == 't' && p[3] == 'i' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_outi, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'p':
		if (p[1] == 'o' && p[2] == 'p' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_pop, 3, tok);
		}
		if (p[1] == 'r' && p[2] == 'o' && p[3] == 'c' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_proc, 4, tok);
		}
		if (p[1] == 'u' && p[2] == 's' && p[3] == 'h' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_push, 4, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'r':
		if (p[1] == 'e' && p[2] == 's' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_res, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_ret, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && p[3] == 'i' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_reti, 4, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && p[3] == 'n' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_retn, 4, tok);
		}
		if (p[1] == 'l' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_rl, 2, tok);
		}
		if (p[1] == 'l' && p[2] == 'a' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rla, 3, tok);
		}
		if (p[1] == 'l' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rlc, 3, tok);
		}
		if (p[1] == 'l' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rld, 3, tok);
		}
		if (p[1] == 'l' && p[2] == 'c' && p[3] == 'a' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_rlca, 4, tok);
		}
		if (p[1] == 'r' && !is_idcnt(p[2])) {
			return z80ic_lexer_keyword(lexer, ztt_rr, 2, tok);
		}
		if (p[1] == 'r' && p[2] == 'a' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rra, 3, tok);
		}
		if (p[1] == 'r' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rrc, 3, tok);
		}
		if (p[1] == 'r' && p[2] == 'c' && p[3] == 'a' &&
		    !is_idcnt(p[4])) {
			return z80ic_lexer_keyword(lexer, ztt_rrca, 4, tok);
		}
		if (p[1] == 'r' && p[2] == 'd' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rrd, 3, tok);
		}
		if (p[1] == 's' && p[2] == 't' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_rst, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 's':
		if (p[1] == 'b' && p[2] == 'c' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_sbc, 3, tok);
		}
		if (p[1] == 'c' && p[2] == 'f' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_scf, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_set, 3, tok);
		}
		if (p[1] == 'l' && p[2] == 'a' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_sla, 3, tok);
		}
		if (p[1] == 'r' && p[2] == 'a' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_sra, 3, tok);
		}
		if (p[1] == 'r' && p[2] == 'l' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_srl, 3, tok);
		}
		if (p[1] == 'u' && p[2] == 'b' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_sub, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'v':
		if (p[1] == 'a' && p[2] == 'r' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_var, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	case 'x':
		if (p[1] == 'o' && p[2] == 'r' && !is_idcnt(p[3])) {
			return z80ic_lexer_keyword(lexer, ztt_xor, 3, tok);
		}
		return z80ic_lexer_invalid(lexer, tok);
	default:
		if (is_num(p[0]))
			return z80ic_lexer_number(lexer, tok);
		if (!is_print(p[0]))
			return z80ic_lexer_nonprint(lexer, tok);
		return z80ic_lexer_invalid(lexer, tok);
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
bool z80ic_lexer_tok_valid_chars(z80ic_lexer_tok_t *tok, size_t offs, size_t *invpos)
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

/** Free token.
 *
 * Free/finalize token obtained via lex_get_tok().
 *
 * @param tok Token
 */
void z80ic_lexer_free_tok(z80ic_lexer_tok_t *tok)
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
const char *z80ic_lexer_str_ttype(z80ic_lexer_toktype_t ttype)
{
	switch (ttype) {
	case ztt_space:
		return "space";
	case ztt_tab:
		return "tab";
	case ztt_newline:
		return "newline";
	case ztt_comment:
		return "'/* ... */'";
	case ztt_lparen:
		return "'('";
	case ztt_rparen:
		return "')'";
	case ztt_comma:
		return "','";
	case ztt_colon:
		return "':'";
	case ztt_scolon:
		return "';'";
	case ztt_period:
		return "'.'";
	case ztt_plus:
		return "'+'";
	case ztt_minus:
		return "'-'";
	case ztt_A:
		return "'A'";
	case ztt_AF:
		return "'AF'";
	case ztt_AF_:
		return "'AF''";
	case ztt_B:
		return "'B'";
	case ztt_BC:
		return "'BC'";
	case ztt_C:
		return "'C'";
	case ztt_D:
		return "'D'";
	case ztt_DE:
		return "'DE'";
	case ztt_E:
		return "'E'";
	case ztt_F:
		return "'F'";
	case ztt_H:
		return "'H'";
	case ztt_HL:
		return "'HL'";
	case ztt_I:
		return "'I'";
	case ztt_IX:
		return "'IX'";
	case ztt_IY:
		return "'IY'";
	case ztt_L:
		return "'L'";
	case ztt_M:
		return "'M'";
	case ztt_NC:
		return "'NC'";
	case ztt_NZ:
		return "'NZ'";
	case ztt_P:
		return "'P'";
	case ztt_PE:
		return "'PE'";
	case ztt_PO:
		return "'PO'";
	case ztt_R:
		return "'R'";
	case ztt_SP:
		return "'SP'";
	case ztt_Z:
		return "'Z'";
	case ztt_add:
		return "'add'";
	case ztt_adc:
		return "'adc'";
	case ztt_and:
		return "'and'";
	case ztt_begin:
		return "'begin'";
	case ztt_bit:
		return "'bit'";
	case ztt_call:
		return "'call'";
	case ztt_ccf:
		return "'ccf'";
	case ztt_cp:
		return "'cp'";
	case ztt_cpd:
		return "'cpd'";
	case ztt_cpdr:
		return "'cpdr'";
	case ztt_cpi:
		return "'cpi'";
	case ztt_cpir:
		return "'cpir'";
	case ztt_cpl:
		return "'cpl'";
	case ztt_daa:
		return "'daa'";
	case ztt_dec:
		return "'dec'";
	case ztt_defb:
		return "'defb'";
	case ztt_defw:
		return "'defw'";
	case ztt_defdw:
		return "'defdw'";
	case ztt_defqw:
		return "'defqw'";
	case ztt_di:
		return "'di'";
	case ztt_djnz:
		return "'djnz'";
	case ztt_ei:
		return "'ei'";
	case ztt_end:
		return "'end'";
	case ztt_ex:
		return "'ex'";
	case ztt_exx:
		return "'exx'";
	case ztt_extern:
		return "'extern'";
	case ztt_global:
		return "'global'";
	case ztt_halt:
		return "'halt'";
	case ztt_im:
		return "'im'";
	case ztt_in:
		return "'in'";
	case ztt_ind:
		return "'ind'";
	case ztt_indr:
		return "'indr'";
	case ztt_ini:
		return "'ini'";
	case ztt_inir:
		return "'inir'";
	case ztt_inc:
		return "'inc'";
	case ztt_jp:
		return "'jp'";
	case ztt_jr:
		return "'jr'";
	case ztt_ld:
		return "'ld'";
	case ztt_ldd:
		return "'ldd'";
	case ztt_lddr:
		return "'lddr'";
	case ztt_ldi:
		return "'ldi'";
	case ztt_ldir:
		return "'ldir'";
	case ztt_lvar:
		return "'lvar'";
	case ztt_neg:
		return "'neg'";
	case ztt_nop:
		return "'nop'";
	case ztt_or:
		return "'ot'";
	case ztt_otdr:
		return "'otdr'";
	case ztt_otir:
		return "'otir'";
	case ztt_out:
		return "'out'";
	case ztt_outd:
		return "'outd'";
	case ztt_outi:
		return "'outi'";
	case ztt_pop:
		return "'pop'";
	case ztt_proc:
		return "'proc'";
	case ztt_push:
		return "'push'";
	case ztt_res:
		return "'res'";
	case ztt_ret:
		return "'ret'";
	case ztt_reti:
		return "'reti'";
	case ztt_retn:
		return "'retn'";
	case ztt_rl:
		return "'rl'";
	case ztt_rla:
		return "'rla'";
	case ztt_rlc:
		return "'rlc'";
	case ztt_rlca:
		return "'rlca'";
	case ztt_rr:
		return "'rr'";
	case ztt_rra:
		return "'rra'";
	case ztt_rrc:
		return "'rrc'";
	case ztt_rrca:
		return "'rrca'";
	case ztt_rld:
		return "'rld'";
	case ztt_rrd:
		return "'rrd'";
	case ztt_rst:
		return "'rst'";
	case ztt_scf:
		return "'scf'";
	case ztt_sbc:
		return "'sbc'";
	case ztt_set:
		return "'set'";
	case ztt_sla:
		return "'sla'";
	case ztt_sra:
		return "'sra'";
	case ztt_srl:
		return "'srl'";
	case ztt_sub:
		return "'sub'";
	case ztt_var:
		return "'var'";
	case ztt_xor:
		return "'xor'";
	case ztt_ident:
		return "id";
	case ztt_number:
		return "num";
	case ztt_eof:
		return "eof";
	case ztt_invalid:
		return "invalid";
	case ztt_invchar:
		return "invchar";
	case ztt_error:
		return "error";
	}

	assert(false);
	return "unknown";
}

/** Print token type.
 *
 * @param ttype Token type
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int z80ic_lexer_print_ttype(z80ic_lexer_toktype_t ttype, FILE *f)
{
	if (fputs(z80ic_lexer_str_ttype(ttype), f) < 0)
		return EIO;

	return EOK;
}

/** Print character, escaping special characters.
 *
 * @param c Character to print
 * @param f Output file
 * @return EOK on success, EIO on I/O error
 */
int z80ic_lexer_dprint_char(char c, FILE *f)
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
static int z80ic_lexer_dprint_str(const char *str, FILE *f)
{
	const char *cp;
	char c;
	int rc;

	cp = str;
	while (*cp != '\0') {
		c = *cp++;
		rc = z80ic_lexer_dprint_char(c, f);
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
static int z80ic_lexer_dprint_tok_range(z80ic_lexer_tok_t *tok, src_pos_t *bpos,
    src_pos_t *epos, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(bpos, epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ":%s", z80ic_lexer_str_ttype(tok->ttype)) < 0)
		return EIO;

	switch (tok->ttype) {
	case ztt_ident:
	case ztt_number:
		if (fprintf(f, ":%s", tok->text) < 0)
			return EIO;
		break;
	case ztt_invalid:
	case ztt_invchar:
		if (fputc(':', f) == EOF)
			return EIO;
		rc = z80ic_lexer_dprint_str(tok->text, f);
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
int z80ic_lexer_dprint_tok(z80ic_lexer_tok_t *tok, FILE *f)
{
	return z80ic_lexer_dprint_tok_range(tok, &tok->bpos, &tok->epos, f);
}

/** Print token structurally (for debugging) pointing to a single character.
 *
 * @param tok Token
 * @param offs Offset of the character to print the range for
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int z80ic_lexer_dprint_tok_chr(z80ic_lexer_tok_t *tok, size_t offs, FILE *f)
{
	src_pos_t pos;
	size_t i;

	pos = tok->bpos;
	for (i = 0; i < offs; i++)
		src_pos_fwd_char(&pos, tok->text[i]);

	return z80ic_lexer_dprint_tok_range(tok, &pos, &pos, f);
}

/** Print token (in original C form).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int z80ic_lexer_print_tok(z80ic_lexer_tok_t *tok, FILE *f)
{
	if (fprintf(f, "%s", tok->text) < 0)
		return EIO;
	return EOK;
}

/** Determine if token type is a comment token.
 *
 * @param itt Token type
 * @return @c true if itt is a comment token type
 */
bool z80ic_lexer_is_comment(z80ic_lexer_toktype_t itt)
{
	return itt == ztt_comment;
}

/** Determine if token type is a whitespace token.
 *
 * @param itt Token type
 * @return @c true if itt is a whitespace token type
 */
bool z80ic_lexer_is_wspace(z80ic_lexer_toktype_t itt)
{
	return itt == ztt_space || itt == ztt_tab || itt == ztt_newline;
}

/** Determine if token type is a reserved word token.
 *
 * @param itt Token type
 * @return @c true if itt is a reserved word token type
 */
bool z80ic_lexer_is_resword(z80ic_lexer_toktype_t itt)
{
	return itt >= ztt_resword_first &&
	    itt <= ztt_resword_last;
}

/** Get value of a number token.
 *
 * @param itok Z80 IC lexer token
 * @param rval Place to store value
 * @return EOK on success, EINVAL if token format is invalid
 */
int z80ic_lexer_number_val(z80ic_lexer_tok_t *itok, int32_t *rval)
{
	const char *text = itok->text;
	int32_t val;

	val = 0;
	while (*text != '\0') {
		if (*text < '0' || *text > '9')
			return EINVAL;

		val = val * 10 + (*text - '0');
		++text;
	}

	*rval = val;
	return EOK;
}
