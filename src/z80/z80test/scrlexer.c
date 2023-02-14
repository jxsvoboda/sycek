/*
 * Copyright 2023 Jiri Svoboda
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
 * IR lexer (lexical analyzer)
 *
 * A lexical analyzer for the intermediate representation (IR) language.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "src_pos.h"
#include "scrlexer.h"

/** Create lexer.
 *
 * @param ops Input ops
 * @param arg Argument to input
 * @param rlexer Place to store pointer to new lexer
 *
 * @return Zero on sucess, ENOMEM if out of memory
 */
int scr_lexer_create(lexer_input_ops_t *ops, void *arg, scr_lexer_t **rlexer)
{
	scr_lexer_t *lexer;

	lexer = calloc(1, sizeof(scr_lexer_t));
	if (lexer == NULL)
		return ENOMEM;

	lexer->input_ops = ops;
	lexer->input_arg = arg;
	*rlexer = lexer;
	return 0;
}

/** Destroy lexer.
 *
 * @param lexer Lexer
 */
void scr_lexer_destroy(scr_lexer_t *lexer)
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
 * at least scr_lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @return Pointer to characters in input buffer.
 */
static char *scr_lexer_chars(scr_lexer_t *lexer)
{
	int rc;
	size_t nread;
	src_pos_t rpos;

	if (!lexer->in_eof && lexer->buf_used - lexer->buf_pos <
	    scr_lexer_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(lexer->buf, lexer->buf + lexer->buf_pos,
		    lexer->buf_used - lexer->buf_pos);
		lexer->buf_used -= lexer->buf_pos;
		lexer->buf_pos = 0;
		/* XX Advance lexer->buf_bpos */

		rc = lexer->input_ops->read(lexer->input_arg, lexer->buf +
		    lexer->buf_used, scr_lexer_buf_size - lexer->buf_used,
		    &nread, &rpos);
		if (rc != 0) {
			printf("read error\n");
		}
		if (nread < scr_lexer_buf_size - lexer->buf_used)
			lexer->in_eof = true;
		if (lexer->buf_used == 0) {
			lexer->buf_bpos = rpos;
			lexer->pos = rpos;
		}
		lexer->buf_used += nread;
		if (lexer->buf_used < scr_lexer_buf_size)
			lexer->buf[lexer->buf_used] = '\0';
	}

	assert(lexer->buf_pos < scr_lexer_buf_size);
	return lexer->buf + lexer->buf_pos;
}

/** Determine if lexer is at end of file.
 *
 * @param lexer Lexer
 * @return @c true iff there are no more characters available
 */
static bool scr_lexer_is_eof(scr_lexer_t *lexer)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = scr_lexer_chars(lexer);
	(void) lc;

	return lexer->buf_pos == lexer->buf_used;
}

/** Get current lexer position in source code.
 *
 * @param lexer Lexer
 * @param pos Place to store position
 */
static void scr_lexer_get_pos(scr_lexer_t *lexer, src_pos_t *pos)
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
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_advance(scr_lexer_t *lexer, size_t nchars, scr_lexer_tok_t *tok)
{
	char *p;

	while (nchars > 0) {
		tok->text = realloc(tok->text, tok->text_size + 2);
		if (tok->text == NULL)
			return ENOMEM;

		p = scr_lexer_chars(lexer);
		tok->text[tok->text_size] = p[0];
		tok->text[tok->text_size + 1] = '\0';
		tok->text_size++;
		++lexer->buf_pos;
		assert(lexer->buf_pos <= scr_lexer_buf_size);
		src_pos_fwd_char(&lexer->pos, p[0]);
		--nchars;
	}

	return 0;
}

/** Lex whitespace.
 *
 * @param lexer Lexer
 * @param stt Token type (one of stt_space, stt_tab, stt_newline)
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_whitespace(scr_lexer_t *lexer, scr_lexer_toktype_t stt,
    scr_lexer_tok_t *tok)
{
	scr_lexer_get_pos(lexer, &tok->bpos);
	scr_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = stt;
	return scr_lexer_advance(lexer, 1, tok);
}

/** Lex comment.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_comment(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	char *p;
	int rc;

	scr_lexer_get_pos(lexer, &tok->bpos);
	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	while (true) {
		rc = scr_lexer_advance(lexer, 1, tok);
		if (rc != 0) {
			scr_lexer_free_tok(tok);
			return rc;
		}

		p = scr_lexer_chars(lexer);
		if (p[0] == '\0') {
			tok->ttype = stt_invalid;
			return 0;
		}

		if (p[0] == '*' && p[1] == '/')
			break;
	}

	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	scr_lexer_get_pos(lexer, &tok->epos);

	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = stt_comment;
	return 0;
}

/** Lex single-character token.
 *
 * @param lexer Lexer
 * @param ttype Token type
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_onechar(scr_lexer_t *lexer, scr_lexer_toktype_t ttype,
    scr_lexer_tok_t *tok)
{
	char *p;

	scr_lexer_get_pos(lexer, &tok->bpos);
	scr_lexer_get_pos(lexer, &tok->epos);
	tok->text = malloc(2);
	p = scr_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	tok->text[0] = p[0];
	tok->text[1] = '\0';
	tok->ttype = ttype;
	return scr_lexer_advance(lexer, 1, tok);
}

/** Lex keyword.
 *
 * @param lexer Lexer
 * @param ttype Token type
 * @param nchars Number of characters in the keyword
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_keyword(scr_lexer_t *lexer, scr_lexer_toktype_t ttype,
    size_t nchars, scr_lexer_tok_t *tok)
{
	char *p;
	int rc;

	scr_lexer_get_pos(lexer, &tok->bpos);

	tok->text = malloc(nchars + 1);
	p = scr_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	memcpy(tok->text, p, nchars);
	tok->text[nchars] = '\0';

	rc = scr_lexer_advance(lexer, nchars - 1, tok);
	if (rc != 0)
		return rc;
	scr_lexer_get_pos(lexer, &tok->epos);
	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = ttype;
	return 0;
}

/** Lex string literal.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_strlit(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	char *p;
	int rc;

	scr_lexer_get_pos(lexer, &tok->bpos);

	while (true) {
		rc = scr_lexer_advance(lexer, 1, tok);
		if (rc != 0) {
			scr_lexer_free_tok(tok);
			return rc;
		}

		p = scr_lexer_chars(lexer);
		if (p[0] == '\0') {
			tok->ttype = stt_invalid;
			return 0;
		}

		if (p[0] == '"')
			break;

		if (p[0] == '\\') {
			/* Skip the next character */
			rc = scr_lexer_advance(lexer, 1, tok);
			if (rc != 0) {
				scr_lexer_free_tok(tok);
				return rc;
			}

			p = scr_lexer_chars(lexer);
			if (p[0] == '\0') {
				tok->ttype = stt_invalid;
				return 0;
			}
		}
	}

	scr_lexer_get_pos(lexer, &tok->epos);
	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = stt_strlit;
	return 0;
}

/** Lex identifier.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_ident(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	char *p;
	int rc;

	scr_lexer_get_pos(lexer, &tok->bpos);
	p = scr_lexer_chars(lexer);
	while (is_idcnt(p[1])) {
		rc = scr_lexer_advance(lexer, 1, tok);
		if (rc != 0) {
			scr_lexer_free_tok(tok);
			return rc;
		}

		p = scr_lexer_chars(lexer);
	}

	scr_lexer_get_pos(lexer, &tok->epos);
	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = stt_ident;
	return 0;
}

/** Lex number.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_number(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	char *p;
	int rc;
	int base = 10;

	scr_lexer_get_pos(lexer, &tok->bpos);
	p = scr_lexer_chars(lexer);

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		rc = scr_lexer_advance(lexer, 2, tok);
		if (rc != 0) {
			scr_lexer_free_tok(tok);
			return rc;
		}

		base = 16;
	}

	p = scr_lexer_chars(lexer);
	while (is_digit(p[1], base)) {
		rc = scr_lexer_advance(lexer, 1, tok);
		if (rc != 0) {
			scr_lexer_free_tok(tok);
			return rc;
		}

		p = scr_lexer_chars(lexer);
	}

	scr_lexer_get_pos(lexer, &tok->epos);

	rc = scr_lexer_advance(lexer, 1, tok);
	if (rc != 0) {
		scr_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = stt_number;
	return 0;
}

/** Lex non-printable character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_nonprint(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	scr_lexer_get_pos(lexer, &tok->bpos);
	scr_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = stt_invchar;
	return scr_lexer_advance(lexer, 1, tok);
}

/** Lex invalid character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_invalid(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	scr_lexer_get_pos(lexer, &tok->bpos);
	scr_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = stt_invalid;
	return scr_lexer_advance(lexer, 1, tok);
}

/** Lex End of File.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return Zero on sucess or non-zero error code
 */
static int scr_lexer_eof(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	scr_lexer_get_pos(lexer, &tok->bpos);
	scr_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = stt_eof;
	return 0;
}

/** Lex next token in.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using scr_lexer_free_tok())
 *
 * @return Zero on sucess or non-zero error code
 */
int scr_lexer_get_tok(scr_lexer_t *lexer, scr_lexer_tok_t *tok)
{
	char *p;

	memset(tok, 0, sizeof(scr_lexer_tok_t));

	p = scr_lexer_chars(lexer);

	/* End of file or null character */
	if (p[0] == '\0') {
		if (scr_lexer_is_eof(lexer))
			return scr_lexer_eof(lexer, tok);
		else
			return scr_lexer_nonprint(lexer, tok);
	}

	switch (p[0]) {
	case '\t':
		return scr_lexer_whitespace(lexer, stt_tab, tok);
	case '\n':
		return scr_lexer_whitespace(lexer, stt_newline, tok);
	case ' ':
		return scr_lexer_whitespace(lexer, stt_space, tok);
	case '"':
		return scr_lexer_strlit(lexer, tok);
	case '%':
		return scr_lexer_ident(lexer, tok);
	case '(':
		return scr_lexer_onechar(lexer, stt_lparen, tok);
	case ')':
		return scr_lexer_onechar(lexer, stt_rparen, tok);
	case ',':
		return scr_lexer_onechar(lexer, stt_comma, tok);
	case '.':
		return scr_lexer_onechar(lexer, stt_period, tok);
	case '/':
		if (p[1] == '*')
			return scr_lexer_comment(lexer, tok);
		return scr_lexer_invalid(lexer, tok);
	case ':':
		return scr_lexer_onechar(lexer, stt_colon, tok);
	case ';':
		return scr_lexer_onechar(lexer, stt_scolon, tok);
	case '@':
		return scr_lexer_ident(lexer, tok);
	case 'A':
		if (p[1] == 'F' && !is_idcnt(p[2])) {
			return scr_lexer_keyword(lexer, stt_AF, 2, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'B':
		if (p[1] == 'C' && !is_idcnt(p[2])) {
			return scr_lexer_keyword(lexer, stt_BC, 2, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'D':
		if (p[1] == 'E' && !is_idcnt(p[2])) {
			return scr_lexer_keyword(lexer, stt_DE, 2, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'H':
		if (p[1] == 'L' && !is_idcnt(p[2])) {
			return scr_lexer_keyword(lexer, stt_HL, 2, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'b':
		if (p[1] == 'y' && p[2] == 't' && p[3] == 'e' &&
		    !is_idcnt(p[4])) {
			return scr_lexer_keyword(lexer, stt_byte, 4, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'c':
		if (p[1] == 'a' && p[2] == 'l' && p[3] == 'l' &&
		    !is_idcnt(p[4])) {
			return scr_lexer_keyword(lexer, stt_call, 4, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'd':
		if (p[1] == 'w' && p[2] == 'o' && p[3] == 'r' &&
		    p[4] == 'd' && !is_idcnt(p[5])) {
			return scr_lexer_keyword(lexer, stt_dword, 5, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'l':
		if (p[1] == 'd' && !is_idcnt(p[2])) {
			return scr_lexer_keyword(lexer, stt_ld, 2, tok);
		}
		if (p[1] == 'd' && p[2] == 'b' && p[3] == 'i' &&
		    p[4] == 'n' && !is_idcnt(p[5])) {
			return scr_lexer_keyword(lexer, stt_ldbin, 5, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'm':
		if (p[1] == 'a' && p[2] == 'p' && p[3] == 'f' &&
		    p[4] == 'i' && p[5] == 'l' && p[6] == 'e' &&
		    !is_idcnt(p[7])) {
			return scr_lexer_keyword(lexer, stt_mapfile, 7, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'p':
		if (p[1] == 'r' && p[2] == 'i' && p[3] == 'n' &&
		    p[4] == 't' && !is_idcnt(p[5])) {
			return scr_lexer_keyword(lexer, stt_print, 5, tok);
		}
		if (p[1] == 't' && p[2] == 'r' && !is_idcnt(p[3])) {
			return scr_lexer_keyword(lexer, stt_ptr, 3, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'q':
		if (p[1] == 'w' && p[2] == 'o' && p[3] == 'r' &&
		    p[4] == 'd' && !is_idcnt(p[5])) {
			return scr_lexer_keyword(lexer, stt_qword, 5, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'v':
		if (p[1] == 'e' && p[2] == 'r' && p[3] == 'i' &&
		    p[4] == 'f' && p[5] == 'y' && !is_idcnt(p[6])) {
			return scr_lexer_keyword(lexer, stt_verify, 6, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case 'w':
		if (p[1] == 'o' && p[2] == 'r' && p[3] == 'd' &&
		    !is_idcnt(p[4])) {
			return scr_lexer_keyword(lexer, stt_word, 4, tok);
		}
		return scr_lexer_invalid(lexer, tok);
	case '{':
		return scr_lexer_onechar(lexer, stt_lbrace, tok);
	case '}':
		return scr_lexer_onechar(lexer, stt_rbrace, tok);
	default:
		if (is_num(p[0]))
			return scr_lexer_number(lexer, tok);
		if (!is_print(p[0]))
			return scr_lexer_nonprint(lexer, tok);
		return scr_lexer_invalid(lexer, tok);
	}

	return 0;
}

/** Determine if token consists only of allowed characters.
 *
 * @param tok Token to validate
 * @param offs Offset at which to start
 * @param invpos Place to store offset of the first invalid character
 * @return @c true if token only consist of valid characters
 */
bool scr_lexer_tok_valid_chars(scr_lexer_tok_t *tok, size_t offs, size_t *invpos)
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
void scr_lexer_free_tok(scr_lexer_tok_t *tok)
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
const char *scr_lexer_str_ttype(scr_lexer_toktype_t ttype)
{
	switch (ttype) {
	case stt_space:
		return "space";
	case stt_tab:
		return "tab";
	case stt_newline:
		return "newline";
	case stt_comment:
		return "'/* ... */'";
	case stt_lparen:
		return "'('";
	case stt_rparen:
		return "')'";
	case stt_lbrace:
		return "'{'";
	case stt_rbrace:
		return "'}'";
	case stt_comma:
		return "','";
	case stt_colon:
		return "':'";
	case stt_scolon:
		return "';'";
	case stt_period:
		return "'.'";
	case stt_AF:
		return "'AF'";
	case stt_BC:
		return "'BC'";
	case stt_DE:
		return "'DE'";
	case stt_HL:
		return "'HL'";
	case stt_byte:
		return "'byte'";
	case stt_call:
		return "'call'";
	case stt_dword:
		return "'dword'";
	case stt_ld:
		return "'ld'";
	case stt_ldbin:
		return "'ldbin'";
	case stt_mapfile:
		return "'mapfile'";
	case stt_print:
		return "'print'";
	case stt_ptr:
		return "'print'";
	case stt_qword:
		return "'qword'";
	case stt_verify:
		return "'verify'";
	case stt_word:
		return "'word'";
	case stt_ident:
		return "id";
	case stt_number:
		return "num";
	case stt_strlit:
		return "strlit";
	case stt_eof:
		return "eof";
	case stt_invalid:
		return "invalid";
	case stt_invchar:
		return "invchar";
	case stt_error:
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
 * @return Zero on sucess, EIO on I/O error
 */
int scr_lexer_print_ttype(scr_lexer_toktype_t ttype, FILE *f)
{
	if (fputs(scr_lexer_str_ttype(ttype), f) < 0)
		return EIO;

	return 0;
}

/** Print character, escaping special characters.
 *
 * @param c Character to print
 * @param f Output file
 * @return Zero on sucess, EIO on I/O error
 */
int scr_lexer_dprint_char(char c, FILE *f)
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

	return 0;
}

/** Print string, escaping special characters.
 *
 * @param str Strint to print
 * @param f Output file
 * @return Zero on sucess, EIO on I/O error
 */
static int scr_lexer_dprint_str(const char *str, FILE *f)
{
	const char *cp;
	char c;
	int rc;

	cp = str;
	while (*cp != '\0') {
		c = *cp++;
		rc = scr_lexer_dprint_char(c, f);
		if (rc != 0)
			return rc;
	}

	return 0;
}

/** Print token structurally (for debugging) with specified range.
 *
 * @param tok Token
 * @param bpos Begin position
 * @param epos End position
 * @param f Output file
 *
 * @return Zero on sucess, EIO on I/O error
 */
static int scr_lexer_dprint_tok_range(scr_lexer_tok_t *tok, src_pos_t *bpos,
    src_pos_t *epos, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(bpos, epos, f);
	if (rc != 0)
		return rc;

	if (fprintf(f, ":%s", scr_lexer_str_ttype(tok->ttype)) < 0)
		return EIO;

	switch (tok->ttype) {
	case stt_ident:
	case stt_number:
		if (fprintf(f, ":%s", tok->text) < 0)
			return EIO;
		break;
	case stt_invalid:
	case stt_invchar:
		if (fputc(':', f) == EOF)
			return EIO;
		rc = scr_lexer_dprint_str(tok->text, f);
		if (rc != 0)
			return EIO;
		break;
	default:
		break;
	}

	if (fprintf(f, ">") < 0)
		return EIO;

	return 0;
}

/** Print token structurally (for debugging).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return Zero on sucess, EIO on I/O error
 */
int scr_lexer_dprint_tok(scr_lexer_tok_t *tok, FILE *f)
{
	return scr_lexer_dprint_tok_range(tok, &tok->bpos, &tok->epos, f);
}

/** Print token structurally (for debugging) pointing to a single character.
 *
 * @param tok Token
 * @param offs Offset of the character to print the range for
 * @param f Output file
 *
 * @return Zero on sucess, EIO on I/O error
 */
int scr_lexer_dprint_tok_chr(scr_lexer_tok_t *tok, size_t offs, FILE *f)
{
	src_pos_t pos;
	size_t i;

	pos = tok->bpos;
	for (i = 0; i < offs; i++)
		src_pos_fwd_char(&pos, tok->text[i]);

	return scr_lexer_dprint_tok_range(tok, &pos, &pos, f);
}

/** Print token (in original C form).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return Zero on sucess, EIO on I/O error
 */
int scr_lexer_print_tok(scr_lexer_tok_t *tok, FILE *f)
{
	if (fprintf(f, "%s", tok->text) < 0)
		return EIO;
	return 0;
}

/** Determine if token type is a comment token.
 *
 * @param stt Token type
 * @return @c true if stt is a comment token type
 */
bool scr_lexer_is_comment(scr_lexer_toktype_t stt)
{
	return stt == stt_comment;
}

/** Determine if token type is a whitespace token.
 *
 * @param stt Token type
 * @return @c true if stt is a whitespace token type
 */
bool scr_lexer_is_wspace(scr_lexer_toktype_t stt)
{
	return stt == stt_space || stt == stt_tab || stt == stt_newline;
}

/** Determine if token type is a reserved word token.
 *
 * @param stt Token type
 * @return @c true if stt is a reserved word token type
 */
bool scr_lexer_is_resword(scr_lexer_toktype_t stt)
{
	return stt >= stt_resword_first &&
	    stt <= stt_resword_last;
}

/** Get value of a number token.
 *
 * @param itok IR lexer token
 * @param rval Place to store value
 * @return Zero on sucess, EINVAL if token format is invalid
 */
int scr_lexer_number_val(scr_lexer_tok_t *itok, int64_t *rval)
{
	const char *text = itok->text;
	int base = 10;
	int digit;
	int64_t val;

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		text += 2;
	}

	val = 0;
	while (*text != '\0') {
		if (!is_digit(*text, base))
			return EINVAL;

		if (*text >= '0' && *text <= '9')
			digit = *text - '0';
		else if (*text >= 'a' && *text <= 'f')
			digit = *text - 'a' + 10;
		else
			digit = *text - 'A' + 10;

		val = val * base + digit;
		++text;
	}

	*rval = val;
	return 0;
}

/** Get text of a string token.
 *
 * @param itok IR lexer token
 * @param rstr Place to store pointer to new string
 * @return Zero on sucess, EINVAL if token format is invalid, ENOMEM
 *         if out of memory.
 */
int scr_lexer_string_text(scr_lexer_tok_t *itok, char **rstr)
{
	const char *text = itok->text;
	char *str = NULL;
	char *dp;

	str = malloc(strlen(text));
	if (str == NULL)
		return ENOMEM;

	assert(*text == '"');
	++text;

	dp = str;
	while (*text != '\0' && *text != '"') {
		if (*text == '\\') {
			++text;
			switch (*text) {
			case '\\':
				*dp++ = '\\';
				break;
			default:
				return EINVAL;
			}
		} else {
			*dp++ = *text;
		}

		++text;
	}

	assert(*text == '"');
	++text;
	assert(*text == '\0');

	*dp = '\0';
	*rstr = str;
	return 0;
}
