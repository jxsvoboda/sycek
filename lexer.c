/*
 * Copyright 2018 Jiri Svoboda
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

#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
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

//		printf("Read input\n");
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
//		printf("Read input done\n");
	}

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
		src_pos_fwd_char(&lexer->pos, p[0]);
		--nchars;
	}

	return EOK;
}

/** Lex whitespace.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_whitespace(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ltt_wspace;
	return lexer_advance(lexer, 1, tok);
}

/** Lex comment.
 *
 * @param lexer Lexer
 * @param tok Output token
 *
 * @return EOK on success or non-zero error code
 */
static int lexer_comment(lexer_t *lexer, lexer_tok_t *tok)
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
	while (p[0] != '*' || p[1] != '/') {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
	}

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

	tok->ttype = ltt_comment;
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
	while (p[1] != '\n' || p[0] == '\\') {
		rc = lexer_advance(lexer, 1, tok);
		if (rc != EOK) {
			lexer_free_tok(tok);
			return rc;
		}

		p = lexer_chars(lexer);
	}

	lexer_get_pos(lexer, &tok->epos);

	/* Skip trailing newline */
	rc = lexer_advance(lexer, 1, tok);
	if (rc != EOK) {
		lexer_free_tok(tok);
		return rc;
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

	/* Preprocessor frament ends with newline, except for backslash-newline */
	while (p[1] != '\n' || p[0] == '\\') {
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

	lexer_get_pos(lexer, &tok->bpos);
	p = lexer_chars(lexer);
	while (is_num(p[1])) {
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

	tok->ttype = ltt_number;
	return EOK;
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

/** Lex next token.
 *
 * @param lexer Lexer
 * @param tok Place to store token (must be freed using lexer_free_tok())
 *
 * @return EOK on success or non-zero error code
 */
int lexer_get_tok(lexer_t *lexer, lexer_tok_t *tok)
{
	char *p;
	memset(tok, 0, sizeof(lexer_tok_t));

	p = lexer_chars(lexer);
//	printf("*p=%c\n", p[0]);
	if (p[0] == '\0')
		return lexer_eof(lexer, tok);

	switch (p[0]) {
	case ' ':
	case '\t':
	case '\n':
		return lexer_whitespace(lexer, tok);
	case '*':
		return lexer_onechar(lexer, ltt_asterisk, tok);
	case '/':
		if (p[1] == '*')
			return lexer_comment(lexer, tok);
		if (p[1] == '/')
			return lexer_dscomment(lexer, tok);
		return lexer_onechar(lexer, ltt_slash, tok);
	case '#':
//		if (1/*XXX*/)
			return lexer_preproc(lexer, tok);
	case '(':
		return lexer_onechar(lexer, ltt_lparen, tok);
	case ')':
		return lexer_onechar(lexer, ltt_rparen, tok);
	case '{':
		return lexer_onechar(lexer, ltt_lbrace, tok);
	case '}':
		return lexer_onechar(lexer, ltt_rbrace, tok);
	case ',':
		return lexer_onechar(lexer, ltt_comma, tok);
	case ';':
		return lexer_onechar(lexer, ltt_scolon, tok);
	case '=':
		return lexer_onechar(lexer, ltt_equals, tok);
	case '[':
		return lexer_onechar(lexer, ltt_lbracket, tok);
	case ']':
		return lexer_onechar(lexer, ltt_rbracket, tok);
	case 'a':
		if (p[1] == 'u' && p[2] == 't' && p[3] == 'o' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_auto, 4, tok);
		}
		return lexer_ident(lexer, tok);
	case 'c':
		if (p[1] == 'h' && p[2] == 'a' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_char, 4, tok);
		}
		if (p[1] == 'o' && p[2] == 'n' && p[3] == 's' &&
		    p[4] == 't' && !is_idcnt(p[5])) {
			return lexer_keyword(lexer, ltt_const, 5, tok);
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
		    p[5] == 't' && p[6] == 'e' && p[7] == 'r' && !is_idcnt(p[8])) {
			return lexer_keyword(lexer, ltt_register, 8, tok);
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
		return lexer_ident(lexer, tok);
	case 't':
		if (p[1] == 'y' && p[2] == 'p' && p[3] == 'e' && p[4] == 'd' &&
		    p[5] == 'e' && p[6] == 'f' && !is_idcnt(p[7])) {
			return lexer_keyword(lexer, ltt_typedef, 7, tok);
		}
		return lexer_ident(lexer, tok);
	case 'u':
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
		return lexer_invalid(lexer, tok);
	}

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
	case ltt_wspace:
		return "ws";
	case ltt_comment:
		return "comment";
	case ltt_dscomment:
		return "dscomment";
	case ltt_preproc:
		return "preproc";
	case ltt_lparen:
		return "(";
	case ltt_rparen:
		return ")";
	case ltt_lbrace:
		return "{";
	case ltt_rbrace:
		return "}";
	case ltt_comma:
		return ",";
	case ltt_scolon:
		return ";";
	case ltt_equals:
		return "=";
	case ltt_asterisk:
		return "*";
	case ltt_slash:
		return "/";
	case ltt_lbracket:
		return "[";
	case ltt_rbracket:
		return "]";
	case ltt_auto:
		return "auto";
	case ltt_char:
		return "char";
	case ltt_const:
		return "const";
	case ltt_do:
		return "do";
	case ltt_double:
		return "double";
	case ltt_enum:
		return "enum";
	case ltt_extern:
		return "extern";
	case ltt_float:
		return "float";
	case ltt_for:
		return "for";
	case ltt_goto:
		return "goto";
	case ltt_if:
		return "if";
	case ltt_inline:
		return "inline";
	case ltt_int:
		return "int";
	case ltt_long:
		return "long";
	case ltt_register:
		return "register";
	case ltt_return:
		return "return";
	case ltt_signed:
		return "signed";
	case ltt_sizeof:
		return "sizeof";
	case ltt_short:
		return "short";
	case ltt_static:
		return "static";
	case ltt_struct:
		return "struct";
	case ltt_typedef:
		return "typedef";
	case ltt_union:
		return "union";
	case ltt_unsigned:
		return "unsigned";
	case ltt_void:
		return "void";
	case ltt_volatile:
		return "volatile";
	case ltt_while:
		return "while";
	case ltt_ident:
		return "id";
	case ltt_number:
		return "num";
	case ltt_eof:
		return "eof";
	case ltt_invalid:
		return "invalid";
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

/** Print token structurally (for debugging).
 *
 * @param tok Token
 * @param f Output file
 *
 * @return EOK on success, EIO on I/O error
 */
int lexer_dprint_tok(lexer_tok_t *tok, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(&tok->bpos, &tok->epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ":%s", lexer_str_ttype(tok->ttype)) < 0)
		return EIO;

	switch (tok->ttype) {
	case ltt_ident:
	case ltt_number:
	case ltt_invalid:
		if (fprintf(f, ":%s", tok->text) < 0)
			return EIO;
		break;
	default:
		break;
	}

	if (fprintf(f, ">") < 0)
		return EIO;

	return EOK;
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
