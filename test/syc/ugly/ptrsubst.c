/*
 * Pointer subscripting with overlong indices
 *
 * These should produce 'conversion may loose significant digits' warnings.
 */

int *p;
long lidx;
long long llidx;
int i;

/* Pointer[long] */
void ptrsubs_long(void)
{
	i = p[lidx];
}

/* Pointer[long long] */
void ptridx_longlong(void)
{
	i = p[llidx];
}
