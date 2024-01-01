/*
 * String literal in expression
 */

char *p;
int *wp;
int sz;
int wsz;

void strptr(void)
{
	p = "Hello";
}

void strsize(void)
{
	sz = sizeof("world");
}

void wstrptr(void)
{
	wp = L"Hello";
}

void wstrsize(void)
{
	wsz = sizeof(L"world");
}
