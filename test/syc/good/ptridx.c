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

/* Pointer += char index */
void ptrinc_char(void)
{
	dp += cidx;
}

/* Pointer - char index */
void ptridx_mchar(void)
{
	dp = p - cidx;
}

/* Pointer -= char index */
void ptrdec_char(void)
{
	dp -= cidx;
}

/* Pointer + int index */
void ptridx_int(void)
{
	dp = p + idx;
}

/* Pointer pre-increment */
void ptr_preinc(void)
{
	dp = ++p;
}

/* Pointer += int index */
void ptrinc_int(void)
{
	dp += idx;
}

/* Pointer - int index */
void ptridx_mint(void)
{
	dp = p - idx;
}

/* Pointer -= int index */
void ptrdec_int(void)
{
	dp -= idx;
}

/* Pointer pre-decrement */
void ptr_predec(void)
{
	dp = --p;
}
