#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

void lexer_destroy(lexer_t *lexer)
{
	if (lexer == NULL)
		return;

	free(lexer);
}

static bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_num(char c)
{
	return (c >= '0' && c <= '9');
}

static bool is_alnum(char c)
{
	return is_alpha(c) || is_num(c);
}

static bool is_idbegin(char c)
{
	return is_alpha(c) || (c == '_');
}

static bool is_idcnt(char c)
{
	return is_alnum(c) || (c == '_');
}

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
		if (lexer->buf_used == 0)
			lexer->buf_bpos = rpos;
		lexer->buf_used += nread;
		if (lexer->buf_used < lexer_buf_size)
			lexer->buf[lexer->buf_used] = '\0';
		lexer->pos = lexer->buf_bpos;
//		printf("Read input done\n");
	}

	return lexer->buf + lexer->buf_pos;
}

static void lexer_get_pos(lexer_t *lexer, src_pos_t *pos)
{
	*pos = lexer->pos;
}

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
		++lexer->pos.col;
		--nchars;
	}

	return EOK;
}

static int lexer_whitespace(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ltt_wspace;
	return lexer_advance(lexer, 1, tok);
}

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

static int lexer_invalid(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	
	tok->ttype = ltt_invalid;
	return lexer_advance(lexer, 1, tok);
}

static int lexer_eof(lexer_t *lexer, lexer_tok_t *tok)
{
	lexer_get_pos(lexer, &tok->bpos);
	lexer_get_pos(lexer, &tok->epos);
	tok->ttype = ltt_eof;
	return EOK;
}

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
	case '(':
		return lexer_onechar(lexer, ltt_lparen, tok);
	case ')':
		return lexer_onechar(lexer, ltt_rparen, tok);
	case '{':
		return lexer_onechar(lexer, ltt_lbrace, tok);
	case '}':
		return lexer_onechar(lexer, ltt_rbrace, tok);
	case ';':
		return lexer_onechar(lexer, ltt_scolon, tok);
	case 'c':
		if (p[1] == 'h' && p[2] == 'a' && p[3] == 'r' &&
		    !is_idcnt(p[4])) {
			return lexer_keyword(lexer, ltt_char, 4, tok);
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

void lexer_free_tok(lexer_tok_t *tok)
{
	if (tok->text != NULL)
		free(tok->text);
	tok->text = NULL;
}

int lexer_dprint_tok(lexer_tok_t *tok, FILE *f)
{
	int rc;

	if (fprintf(f, "<") < 0)
		return EIO;
	rc = src_pos_print_range(&tok->bpos, &tok->epos, f);
	if (rc != EOK)
		return rc;

	if (fprintf(f, ":") < 0)
		return EIO;

	switch (tok->ttype) {
	case ltt_wspace:
		if (fprintf(f, "ws") < 0)
			return EIO;
		break;
	case ltt_lparen:
		if (fprintf(f, "(") < 0)
			return EIO;
		break;
	case ltt_rparen:
		if (fprintf(f, ")") < 0)
			return EIO;
		break;
	case ltt_lbrace:
		if (fprintf(f, "{") < 0)
			return EIO;
		break;
	case ltt_rbrace:
		if (fprintf(f, "}") < 0)
			return EIO;
		break;
	case ltt_scolon:
		if (fprintf(f, ";") < 0)
			return EIO;
		break;
	case ltt_char:
		if (fprintf(f, "char") < 0)
			return EIO;
		break;
	case ltt_do:
		if (fprintf(f, "do") < 0)
			return EIO;
		break;
	case ltt_double:
		if (fprintf(f, "double") < 0)
			return EIO;
		break;
	case ltt_enum:
		if (fprintf(f, "enum") < 0)
			return EIO;
		break;
	case ltt_extern:
		if (fprintf(f, "extern") < 0)
			return EIO;
		break;
	case ltt_float:
		if (fprintf(f, "float") < 0)
			return EIO;
		break;
	case ltt_for:
		if (fprintf(f, "for") < 0)
			return EIO;
		break;
	case ltt_goto:
		if (fprintf(f, "goto") < 0)
			return EIO;
		break;
	case ltt_if:
		if (fprintf(f, "if") < 0)
			return EIO;
		break;
	case ltt_inline:
		if (fprintf(f, "inline") < 0)
			return EIO;
		break;
	case ltt_int:
		if (fprintf(f, "int") < 0)
			return EIO;
		break;
	case ltt_long:
		if (fprintf(f, "long") < 0)
			return EIO;
		break;
	case ltt_register:
		if (fprintf(f, "register") < 0)
			return EIO;
		break;
	case ltt_return:
		if (fprintf(f, "return") < 0)
			return EIO;
		break;
	case ltt_signed:
		if (fprintf(f, "signed") < 0)
			return EIO;
		break;
	case ltt_sizeof:
		if (fprintf(f, "sizeof") < 0)
			return EIO;
		break;
	case ltt_short:
		if (fprintf(f, "short") < 0)
			return EIO;
		break;
	case ltt_static:
		if (fprintf(f, "static") < 0)
			return EIO;
		break;
	case ltt_struct:
		if (fprintf(f, "struct") < 0)
			return EIO;
		break;
	case ltt_typedef:
		if (fprintf(f, "typedef") < 0)
			return EIO;
		break;
	case ltt_union:
		if (fprintf(f, "union") < 0)
			return EIO;
		break;
	case ltt_unsigned:
		if (fprintf(f, "unsigned") < 0)
			return EIO;
		break;
	case ltt_void:
		if (fprintf(f, "void") < 0)
			return EIO;
		break;
	case ltt_volatile:
		if (fprintf(f, "volatile") < 0)
			return EIO;
		break;
	case ltt_while:
		if (fprintf(f, "while") < 0)
			return EIO;
		break;
	case ltt_ident:
		if (fprintf(f, "id:%s", tok->text) < 0)
			return EIO;
		break;
	case ltt_number:
		if (fprintf(f, "num:%s", tok->text) < 0)
			return EIO;
		break;
	case ltt_eof:
		if (fprintf(f, "eof") < 0)
			return EIO;
		break;
	case ltt_invalid:
		if (fprintf(f, "invalid") < 0)
			return EIO;
		break;
	}

	if (fprintf(f, ">") < 0)
		return EIO;

	return EOK;
}

int lexer_print_tok(lexer_tok_t *tok, FILE *f)
{
	if (fprintf(f, "%s", tok->text) < 0)
		return EIO;
	return EOK;
}
