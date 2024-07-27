/*
 * Variable arguments
 */

__va_list x;

void vfoo(__va_list ap)
{
	(void)ap;
}

void foo(char c, ...)
{
	__va_list ap;
	(void)c;
	vfoo(ap);
}

void main(void)
{
	foo('c');
}
