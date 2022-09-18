/*
 * Return without a value in function returning non-void.
 */
int foo(void)
{
	return;
}

/*
 * Return with a value in function returning void.
 */
void bar(void)
{
	return foo();
}
