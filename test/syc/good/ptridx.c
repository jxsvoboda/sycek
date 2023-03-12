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

/* Pointer - char index */
void ptridx_mchar(void)
{
	dp = p - cidx;
}

/* Pointer + int index */
void ptridx_int(void)
{
	dp = p + idx;
}

/* Pointer - int index */
void ptridx_mint(void)
{
	dp = p - idx;
}
