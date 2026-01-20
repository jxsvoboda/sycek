/*
 * Atrribute may_ignore_return
 */

int foo(int i) __attribute__((may_ignore_return))
{
	/* Return something useless. */
	return i;
}

void bar(void)
{
	/* This will not produce a warning, return value can be ignored. */
	foo(1);
}
