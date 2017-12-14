/*
 * Lexer input from string
 *
 * Used for testing
 */

#include <merrno.h>
#include <src_pos.h>
#include <string.h>
#include <str_input.h>

static int str_lexer_read(void *, char *, size_t, size_t *, src_pos_t *);

lexer_input_ops_t lexer_str_input = {
	.read = str_lexer_read
};

/** Lexer input form a string constant. */
static int str_lexer_read(void *arg, char *buf, size_t bsize, size_t *nread,
    src_pos_t *bpos)
{
	str_input_t *sinput = (str_input_t *)arg;
	size_t len;
	size_t i;

//	printf("str_lexer_read\n");
	len = strlen(sinput->str + sinput->pos);
//	printf("str_lexer_read: bsize=%zu len=%zu\n", bsize, len);
	if (bsize < len)
		len = bsize;

	memcpy(buf, sinput->str + sinput->pos, len);
	*nread = len;
	*bpos = sinput->cpos;

	/* Advance source position */
	for (i = 0; i < len; i++)
		src_pos_fwd_char(&sinput->cpos, sinput->str[sinput->pos++]);

	printf("str_lexer_read: pos=");
	src_pos_print_range(&sinput->cpos, &sinput->cpos, stdout);
	printf("\n");

	return EOK;
}

void str_input_init(str_input_t *sinput, const char *s)
{
	sinput->str = s;
	sinput->pos = 0;

	src_pos_set(&sinput->cpos, "none", 1, 1);
}
