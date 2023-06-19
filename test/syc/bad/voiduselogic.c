/*
 * Invalid use of void value. (where truth value is expected)
 */

void retvoid(void)
{
}

int foo(void)
{
	if (retvoid())
		return;
}
