/*
 * Pointer indexing
 */

int *p;
char cidx;
int idx;
int *dp;

/* Pointer + char index */
void ptridx_char(void)
{
	dp = p + cidx;
}

/* Pointer + int index */
void ptridx_int(void)
{
	dp = p + idx;
}
