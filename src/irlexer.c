/*
 * Copyright 2024 Jiri Svoboda
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
#include <irlexer.h>
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
int ir_lexer_create(lexer_input_ops_t *ops, void *arg, ir_lexer_t **rlexer)
{
	ir_lexer_t *lexer;

	lexer = calloc(1, sizeof(ir_lexer_t));
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
void ir_lexer_destroy(ir_lexer_t *lexer)
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
 * at least ir_lexer_buf_low_watermark valid characters (unless at EOF).
 *
 * @return Pointer to characters in input buffer.
 */
static char *ir_lexer_chars(ir_lexer_t *lexer)
{
	int rc;
	size_t nread;
	src_pos_t rpos;

	if (!lexer->in_eof && lexer->buf_used - lexer->buf_pos <
	    ir_lexer_buf_low_watermark) {
		/* Move data to beginning of buffer */
		memmove(lexer->buf, lexer->buf + lexer->buf_pos,
		    lexer->buf_used - lexer->buf_pos);
		lexer->buf_used -= lexer->buf_pos;
		lexer->buf_pos = 0;
		/* XX Advance lexer->buf_bpos */

		rc = lexer->input_ops->read(lexer->input_arg, lexer->buf +
		    lexer->buf_used, ir_lexer_buf_size - lexer->buf_used,
		    &nread, &rpos);
		if (rc != EOK) {
			printf("read error\n");
		}
		if (nread < ir_lexer_buf_size - lexer->buf_used)
			lexer->in_eof = true;
		if (lexer->buf_used == 0) {
			lexer->buf_bpos = rpos;
			lexer->pos = rpos;
		}
		lexer->buf_used += nread;
		if (lexer->buf_used < ir_lexer_buf_size)
			lexer->buf[lexer->buf_used] = '\0';
	}

	assert(lexer->buf_pos < ir_lexer_buf_size);
	return lexer->buf + lexer->buf_pos;
}

/** Determine if lexer is at end of file.
 *
 * @param lexer Lexer
 * @return @c true iff there are no more characters available
 */
static bool ir_lexer_is_eof(ir_lexer_t *lexer)
{
	char *lc;

	/* Make sure buffer is filled, if possible */
	lc = ir_lexer_chars(lexer);
	(void) lc;

	return lexer->buf_pos == lexer->buf_used;
}

/** Get current lexer position in source code.
 *
 * @param lexer Lexer
 * @param pos Place to store position
 */
static void ir_lexer_get_pos(ir_lexer_t *lexer, src_pos_t *pos)
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
static int ir_lexer_advance(ir_lexer_t *lexer, size_t nchars, ir_lexer_tok_t *tok)
{
	char *p;
	char *ntext;

	while (nchars > 0) {
		ntext = realloc(tok->text, tok->text_size + 2);
		if (ntext == NULL)
			return ENOMEM;

		tok->text = ntext;

		p = ir_lexer_chars(lexer);
		tok->text[tok->text_size] = p[0];
		tok->text[tok->text_size + 1] = '\0';
		tok->text_size++;
		++lexer->buf_pos;
		assert(lexer->buf_pos <= ir_lexer_buf_size);
		src_pos_fwd_char(&lexer->pos, p[0]);
		--nchars;
	}

	return EOK;
}

/** Lex whitespace.
 *
 * @param lexer Lexer
 * @param itt Token type (one of itt_space, itt_tab, itt_newline)
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_whitespace(ir_lexer_t *lexer, ir_lexer_toktype_t itt,
    ir_lexer_tok_t *tok)
{
	ir_lexer_get_pos(lexer, &tok->bpos);
	ir_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = itt;
	return ir_lexer_advance(lexer, 1, tok);
}

/** Lex comment.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_comment(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	char *p;
	int rc;

	ir_lexer_get_pos(lexer, &tok->bpos);
	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
		return rc;
	}

	while (true) {
		rc = ir_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			ir_lexer_free_tok(tok);
			return rc;
		}

		p = ir_lexer_chars(lexer);
		if (p[0] == '\0') {
			tok->ttype = itt_invalid;
			return EOK;
		}

		if (p[0] == '*' && p[1] == '/')
			break;
	}

	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
		return rc;
	}

	ir_lexer_get_pos(lexer, &tok->epos);

	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = itt_comment;
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
static int ir_lexer_onechar(ir_lexer_t *lexer, ir_lexer_toktype_t ttype,
    ir_lexer_tok_t *tok)
{
	char *p;

	ir_lexer_get_pos(lexer, &tok->bpos);
	ir_lexer_get_pos(lexer, &tok->epos);
	tok->text = malloc(2);
	p = ir_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	tok->text[0] = p[0];
	tok->text[1] = '\0';
	tok->ttype = ttype;
	return ir_lexer_advance(lexer, 1, tok);
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
static int ir_lexer_keyword(ir_lexer_t *lexer, ir_lexer_toktype_t ttype,
    size_t nchars, ir_lexer_tok_t *tok)
{
	char *p;
	int rc;

	ir_lexer_get_pos(lexer, &tok->bpos);

	tok->text = malloc(nchars + 1);
	p = ir_lexer_chars(lexer);
	if (tok->text == NULL)
		return ENOMEM;
	memcpy(tok->text, p, nchars);
	tok->text[nchars] = '\0';

	rc = ir_lexer_advance(lexer, nchars - 1, tok);
	if (rc != EOK)
		return rc;
	ir_lexer_get_pos(lexer, &tok->epos);
	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
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
static int ir_lexer_ident(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	char *p;
	int rc;

	ir_lexer_get_pos(lexer, &tok->bpos);
	p = ir_lexer_chars(lexer);
	while (is_idcnt(p[1])) {
		rc = ir_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			ir_lexer_free_tok(tok);
			return rc;
		}

		p = ir_lexer_chars(lexer);
	}

	ir_lexer_get_pos(lexer, &tok->epos);
	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = itt_ident;
	return EOK;
}

/** Lex number.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_number(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	char *p;
	int rc;

	ir_lexer_get_pos(lexer, &tok->bpos);
	p = ir_lexer_chars(lexer);

	while (is_digit(p[1], 10)) {
		rc = ir_lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			ir_lexer_free_tok(tok);
			return rc;
		}

		p = ir_lexer_chars(lexer);
	}

	ir_lexer_get_pos(lexer, &tok->epos);

	rc = ir_lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		ir_lexer_free_tok(tok);
		return rc;
	}

	tok->ttype = itt_number;
	return EOK;
}

/** Lex non-printable character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_nonprint(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	ir_lexer_get_pos(lexer, &tok->bpos);
	ir_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = itt_invchar;
	return ir_lexer_advance(lexer, 1, tok);
}

/** Lex invalid character.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_invalid(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	ir_lexer_get_pos(lexer, &tok->bpos);
	ir_lexer_get_pos(lexer, &tok->epos);

	tok->ttype = itt_invalid;
	return ir_lexer_advance(lexer, 1, tok);
}

/** Lex End of File.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int ir_lexer_eof(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	ir_lexer_get_pos(lexer, &tok->bpos);
	ir_lexer_get_pos(lexer, &tok->epos);
	tok->ttype = itt_eof;
	return EOK;
}

/** Lex next token in.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using ir_lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
int ir_lexer_get_tok(ir_lexer_t *lexer, ir_lexer_tok_t *tok)
{
	char *p;

	memset(tok, 0, sizeof(ir_lexer_tok_t));

	p = ir_lexer_chars(lexer);

	/* End of file or null character */
	if (p[0] == '\0') {
		if (ir_lexer_is_eof(lexer))
			return ir_lexer_eof(lexer, tok);
		else
			return ir_lexer_nonprint(lexer, tok);
	}

	switch (p[0]) {
	case '\t':
		return ir_lexer_whitespace(lexer, itt_tab, tok);
	case '\n':
		return ir_lexer_whitespace(lexer, itt_newline, tok);
	case ' ':
		return ir_lexer_whitespace(lexer, itt_space, tok);
	case '%':
		return ir_lexer_ident(lexer, tok);
	case '(':
		return ir_lexer_onechar(lexer, itt_lparen, tok);
	case ')':
		return ir_lexer_onechar(lexer, itt_rparen, tok);
	case ',':
		return ir_lexer_onechar(lexer, itt_comma, tok);
	case '.':
		if (p[1] == '.' && p[2] == '.')
			return ir_lexer_keyword(lexer, itt_ellipsis, 3, tok);
		return ir_lexer_onechar(lexer, itt_period, tok);
	case '/':
		if (p[1] == '*')
			return ir_lexer_comment(lexer, tok);
		return ir_lexer_invalid(lexer, tok);
	case ':':
		return ir_lexer_onechar(lexer, itt_colon, tok);
	case ';':
		return ir_lexer_onechar(lexer, itt_scolon, tok);
	case '@':
		return ir_lexer_ident(lexer, tok);
	case 'a':
		if (p[1] == 'd' && p[2] == 'd' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_add, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'd' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_and, 3, tok);
		}
		if (p[1] == 't' && p[2] == 't' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_attr, 4, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'b':
		if (p[1] == 'e' && p[2] == 'g' && p[3] == 'i' &&
		    p[4] == 'n' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_begin, 5, tok);
		}
		if (p[1] == 'n' && p[2] == 'o' && p[3] == 't' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_bnot, 4, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'c':
		if (p[1] == 'a' && p[2] == 'l' && p[3] == 'l' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_call, 4, tok);
		}
		if (p[1] == 'a' && p[2] == 'l' && p[3] == 'l' &&
		    p[4] == 'i' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_calli, 5, tok);
		}
		if (p[1] == 'o' && p[2] == 'p' && p[3] == 'y' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_copy, 4, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'e':
		if (p[1] == 'n' && p[2] == 'd' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_end, 3, tok);
		}
		if (p[1] == 'q' && !is_idcnt(p[2])) {
			return ir_lexer_keyword(lexer, itt_eq, 2, tok);
		}
		if (p[1] == 'x' && p[2] == 't' && p[3] == 'e' &&
		    p[4] == 'r' && p[5] == 'n' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_extern, 6, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'g':
		if (p[1] == 'l' && p[2] == 'o' && p[3] == 'b' &&
		    p[4] == 'a' && p[5] == 'l' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_global, 6, tok);
		}
		if (p[1] == 't' && !is_idcnt(p[2])) {
			return ir_lexer_keyword(lexer, itt_gt, 2, tok);
		}
		if (p[1] == 't' && p[2] == 'u' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_gtu, 3, tok);
		}
		if (p[1] == 't' && p[2] == 'e' && p[3] == 'q' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_gteq, 4, tok);
		}
		if (p[1] == 't' && p[2] == 'e' && p[3] == 'u' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_gteu, 4, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'i':
		if (p[1] == 'm' && p[2] == 'm' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_imm, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 't' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_int, 3, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'j':
		if (p[1] == 'm' && p[2] == 'p' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_jmp, 3, tok);
		}
		if (p[1] == 'n' && p[2] == 'z' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_jnz, 3, tok);
		}
		if (p[1] == 'z' && !is_idcnt(p[2])) {
			return ir_lexer_keyword(lexer, itt_jz, 2, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'l':
		if (p[1] == 't' && !is_idcnt(p[2])) {
			return ir_lexer_keyword(lexer, itt_lt, 2, tok);
		}
		if (p[1] == 't' && p[2] == 'u' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_ltu, 3, tok);
		}
		if (p[1] == 't' && p[2] == 'e' && p[3] == 'q' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_lteq, 4, tok);
		}
		if (p[1] == 't' && p[2] == 'e' && p[3] == 'u' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_lteu, 4, tok);
		}
		if (p[1] == 'v' && p[2] == 'a' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_lvar, 4, tok);
		}
		if (p[1] == 'v' && p[2] == 'a' && p[3] == 'r' &&
		    p[4] == 'p' && p[5] == 't' && p[6] == 'r' &&
		    !is_idcnt(p[7])) {
			return ir_lexer_keyword(lexer, itt_lvarptr, 7, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'm':
		if (p[1] == 'u' && p[2] == 'l' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_mul, 3, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'n':
		if (p[1] == 'e' && p[2] == 'g' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_neg, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 'q' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_neq, 3, tok);
		}
		if (p[1] == 'i' && p[2] == 'l' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_nil, 3, tok);
		}
		if (p[1] == 'o' && p[2] == 'p' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_nop, 3, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'o':
		if (p[1] == 'r' && !is_idcnt(p[2])) {
			return ir_lexer_keyword(lexer, itt_or, 2, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'p':
		if (p[1] == 'r' && p[2] == 'o' && p[3] == 'c' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_proc, 4, tok);
		}
		if (p[1] == 't' && p[2] == 'r' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_ptr, 3, tok);
		}
		if (p[1] == 't' && p[2] == 'r' && p[3] == 'i' &&
		    p[4] == 'd' && p[5] == 'x' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_ptridx, 6, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'r':
		if (p[1] == 'e' && p[2] == 'a' && p[3] == 'd' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_read, 4, tok);
		}
		if (p[1] == 'e' && p[2] == 'c' && p[3] == 'c' &&
		    p[4] == 'o' && p[5] == 'p' && p[6] == 'y' &&
		    !is_idcnt(p[7])) {
			return ir_lexer_keyword(lexer, itt_reccopy, 7, tok);
		}
		if (p[1] == 'e' && p[2] == 'c' && p[3] == 'o' &&
		    p[4] == 'r' && p[5] == 'd' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_record, 6, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_ret, 3, tok);
		}
		if (p[1] == 'e' && p[2] == 't' && p[3] == 'v' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_retv, 4, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 's':
		if (p[1] == 'g' && p[2] == 'n' && p[3] == 'e' &&
		    p[4] == 'x' && p[5] == 't' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_sgnext, 6, tok);
		}
		if (p[1] == 'h' && p[2] == 'l' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_shl, 3, tok);
		}
		if (p[1] == 'h' && p[2] == 'r' && p[3] == 'a' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_shra, 4, tok);
		}
		if (p[1] == 'h' && p[2] == 'r' && p[3] == 'l' &&
		    !is_idcnt(p[4])) {
			return ir_lexer_keyword(lexer, itt_shrl, 4, tok);
		}
		if (p[1] == 'u' && p[2] == 'b' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_sub, 3, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 't':
		if (p[1] == 'r' && p[2] == 'u' && p[3] == 'n' &&
		    p[4] == 'c' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_trunc, 5, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'u':
		if (p[1] == 'n' && p[2] == 'i' && p[3] == 'o' &&
		    p[4] == 'n' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_union, 5, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'v':
		if (p[1] == 'a' && p[2] == 'r' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_var, 3, tok);
		}
		if (p[1] == 'a' && p[2] == 'r' && p[3] == 'p' &&
		    p[4] == 't' && p[5] == 'r' && !is_idcnt(p[6])) {
			return ir_lexer_keyword(lexer, itt_varptr, 6, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'w':
		if (p[1] == 'r' && p[2] == 'i' && p[3] == 't' &&
		    p[4] == 'e' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_write, 5, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'x':
		if (p[1] == 'o' && p[2] == 'r' && !is_idcnt(p[3])) {
			return ir_lexer_keyword(lexer, itt_xor, 3, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case 'z':
		if (p[1] == 'r' && p[2] == 'e' && p[3] == 'x' &&
		    p[4] == 't' && !is_idcnt(p[5])) {
			return ir_lexer_keyword(lexer, itt_zrext, 5, tok);
		}
		return ir_lexer_invalid(lexer, tok);
	case '{':
		return ir_lexer_onechar(lexer, itt_lbrace, tok);
	case '}':
		return ir_lexer_onechar(lexer, itt_rbrace, tok);
	default:
		if (is_num(p[0]))
			return ir_lexer_number(lexer, tok);
		if (!is_print(p[0]))
			return ir_lexer_nonprint(lexer, tok);
		return ir_lexer_invalid(lexer, tok);
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
bool ir_lexer_tok_valid_chars(ir_lexer_tok_t *tok, size_t offs, size_t *invpos)
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
void ir_lexer_free_tok(ir_lexer_tok_t *tok)
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
const char *ir_lexer_str_ttype(ir_lexer_toktype_t ttype)
{
	switch (ttype) {
	case itt_space:
		return "space";
	case itt_tab:
		return "tab";
	case itt_newline:
		return "newline";
	case itt_comment:
		return "'/* ... */'";
	case itt_lparen:
		return "'('";
	case itt_rparen:
		return "')'";
	case itt_lbrace:
		return "'{'";
	case itt_rbrace:
		return "'}'";
	case itt_comma:
		return "','";
	case itt_colon:
		return "':'";
	case itt_scolon:
		return "';'";
	case itt_period:
		return "'.'";
	case itt_ellipsis:
		return "'...'";
	case itt_add:
		return "'add'";
	case itt_and:
		return "'and'";
	case itt_attr:
		return "'attr'";
	case itt_begin:
		return "'begin'";
	case itt_bnot:
		return "'bnot'";
	case itt_call:
		return "'call'";
	case itt_calli:
		return "'calli'";
	case itt_copy:
		return "'copy'";
	case itt_end:
		return "'end'";
	case itt_eq:
		return "'eq'";
	case itt_extern:
		return "'extern'";
	case itt_global:
		return "'global'";
	case itt_gt:
		return "'gt'";
	case itt_gtu:
		return "'gtu'";
	case itt_gteq:
		return "'gteq'";
	case itt_gteu:
		return "'gteu'";
	case itt_imm:
		return "'imm'";
	case itt_int:
		return "'int'";
	case itt_jmp:
		return "'jmp'";
	case itt_jnz:
		return "'jnz'";
	case itt_jz:
		return "'jz'";
	case itt_lt:
		return "'lt'";
	case itt_ltu:
		return "'ltu'";
	case itt_lteq:
		return "'lteq'";
	case itt_lteu:
		return "'lteu'";
	case itt_lvar:
		return "'lvar'";
	case itt_lvarptr:
		return "'lvarptr'";
	case itt_mul:
		return "'mul'";
	case itt_neg:
		return "'neg'";
	case itt_neq:
		return "'neq'";
	case itt_nil:
		return "'nil'";
	case itt_nop:
		return "'nop'";
	case itt_or:
		return "'or'";
	case itt_proc:
		return "'proc'";
	case itt_ptr:
		return "'ptr'";
	case itt_ptridx:
		return "'ptridx'";
	case itt_read:
		return "'read'";
	case itt_reccopy:
		return "'reccopy'";
	case itt_record:
		return "'record'";
	case itt_ret:
		return "'ret'";
	case itt_retv:
		return "'retv'";
	case itt_sgnext:
		return "'sgnext'";
	case itt_shl:
		return "'shl'";
	case itt_shra:
		return "'shra'";
	case itt_shrl:
		return "'shrl'";
	case itt_sub:
		return "'sub'";
	case itt_trunc:
		return "'trunc'";
	case itt_union:
		return "'union'";
	case itt_var:
		return "'var'";
	case itt_varptr:
		return "'varptr'";
	case itt_write:
		return "'write'";
	case itt_xor:
		return "'xor'";
	case itt_zrext:
		return "'zrext'";
	case itt_ident:
		return "id";
	case itt_number:
		return "num";
	case itt_eof:
		return "eof";
	case itt_invalid:
		return "invalid";
	case itt_invchar:
		return "invchar";
	case itt_error:
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
int ir_lexer_print_ttype(ir_lexer_toktype_t ttype, FILE *f)
{
	if (fputs(ir_lexer_str_ttype(ttype), f) < 0)
		return EIO;

	return EOK;
}

/** Print character, escaping special characters.
 *
 * @param c Character to print
 * @param f Output file
 * @return EOK on success, EIO on I/O error
 */
int ir_lexer_dprint_char(char c, FILE *f)
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
static int ir_lexer_dprint_str(const char *str, FILE *f)
{
	const char *cp;
	char c;
	int rc;

	cp = str;
	while (*cp != '\0') {
		c = *cp++;
		rc = ir_lexer_dprint_char(c, f);
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
static int ir_lexer_dprint_tok_range(ir_lexer_tok_t *tok, src_pos_t *bpos,
    src_pos_t *epos, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(bpos, epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ":%s", ir_lexer_str_ttype(tok->ttype)) < 0)
		return EIO;

	switch (tok->ttype) {
	case itt_ident:
	case itt_number:
		if (fprintf(f, ":%s", tok->text) < 0)
			return EIO;
		break;
	case itt_invalid:
	case itt_invchar:
		if (fputc(':', f) == EOF)
			return EIO;
		rc = ir_lexer_dprint_str(tok->text, f);
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
int ir_lexer_dprint_tok(ir_lexer_tok_t *tok, FILE *f)
{
	return ir_lexer_dprint_tok_range(tok, &tok->bpos, &tok->epos, f);
}

/** Print token structurally (for debugging) pointing to a single character.
 *
 * @param tok Token
 * @param offs Offset of the character to print the range for
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int ir_lexer_dprint_tok_chr(ir_lexer_tok_t *tok, size_t offs, FILE *f)
{
	src_pos_t pos;
	size_t i;

	pos = tok->bpos;
	for (i = 0; i < offs; i++)
		src_pos_fwd_char(&pos, tok->text[i]);

	return ir_lexer_dprint_tok_range(tok, &pos, &pos, f);
}

/** Print token (in original C form).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int ir_lexer_print_tok(ir_lexer_tok_t *tok, FILE *f)
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
bool ir_lexer_is_comment(ir_lexer_toktype_t itt)
{
	return itt == itt_comment;
}

/** Determine if token type is a whitespace token.
 *
 * @param itt Token type
 * @return @c true if itt is a whitespace token type
 */
bool ir_lexer_is_wspace(ir_lexer_toktype_t itt)
{
	return itt == itt_space || itt == itt_tab || itt == itt_newline;
}

/** Determine if token type is a reserved word token.
 *
 * @param itt Token type
 * @return @c true if itt is a reserved word token type
 */
bool ir_lexer_is_resword(ir_lexer_toktype_t itt)
{
	return itt >= itt_resword_first &&
	    itt <= itt_resword_last;
}

/** Get value of a number token.
 *
 * @param itok IR lexer token
 * @param rval Place to store value
 * @return EOK on success, EINVAL if token format is invalid
 */
int ir_lexer_number_val(ir_lexer_tok_t *itok, int32_t *rval)
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
