/*
 * Pointer indexing
 */

int *p;
char cidx;
int idx;
int *dp;

/* Pointer + char index */
int *ptridx_char(void)
{
	dp = p + cidx;
}

/* Pointer + int index */
int *ptridx_int(void)
{
	dp = p + idx;
}
