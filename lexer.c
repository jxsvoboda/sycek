#include <errno.h>
#include <lexer.h>
#include <merrno.h>
#include <src_pos.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int lexer_create(lexer_input_ops_t *ops, void *arg, lexer_t **rlexer)
{
	lexer_t *lexer;

	(void) ops;
	(void) arg;
	(void) rlexer;

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

	if (lexer->buf_used == 0/*XXX*/) {
		printf("Read input\n");
		rc = lexer->input_ops->read(lexer->input_arg, lexer->buf,
		    lexer_buf_size, &lexer->buf_used, &lexer->buf_bpos);
		if (rc != EOK) {
			printf("read error\n");
		}
		lexer->pos = lexer->buf_bpos;
		printf("Read input done\n");
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
	printf("*p=%c\n", p[0]);
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
	case 'i':
		if (p[1] == 'n' && p[2] == 't' && !is_idcnt(p[3]))
			return lexer_keyword(lexer, ltt_int, 3, tok);
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
	case ltt_int:
		if (fprintf(f, "int") < 0)
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
