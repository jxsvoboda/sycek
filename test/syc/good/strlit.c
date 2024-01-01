/*
 * String literal in expression
 */

char *p;
int sz;

void strptr(void)
{
	p = "Hello";
}

void strsize(void)
{
	sz = sizeof("world");
}
