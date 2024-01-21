/*
 * Conditional with void operands can be rewritten as an if-else statement.
 */

int c;
int d;

void foo(void)
{
}

void bar(void)
{
}

void cond(void)
{
	c != 0 ? foo() : bar();
}
