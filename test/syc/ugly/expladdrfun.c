/*
 * Explicitly taking the address of a function is not necessary.
 */

void foo(void)
{
}

void (*fp)(void);

void main(void)
{
	fp = &foo;
}
