/*
 * Pointer indexing with overlong indices
 *
 * These should produce 'conversion may loose significant digits' warnings.
 */

int *p;
long lidx;
long long llidx;
int *dp;

/* Pointer + long index */
void ptridx_long(void)
{
	dp = p + lidx;
}

/* Pointer + long long index */
void ptridx_longlong(void)
{
	dp = p + llidx;
}
