/*
 * Explicitly dereferencing function pointer is not necessary
 */

void foo(void)
{
}

void (*fp)(void) = foo;

void main(void)
{
	(*fp)();
}
