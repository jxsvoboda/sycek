/*
 * Sizeof operator applied to a function
 */

void foo(void)
{
}

int szfoo;

void main(void)
{
	szfoo = sizeof(foo);
}
