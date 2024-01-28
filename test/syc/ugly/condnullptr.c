/*
 * Conditional with one operand being 0 used as a null pointer constant.
 *
 * This is allowed by the C standard, but we will issue warning:
 * zero used as a null pointer constant.
 */

int c;
int *pa;
void *pd;

void cond(void)
{
	pd = c != 0 ? pa : 0;
}
