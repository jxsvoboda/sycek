void nonvar(char *str)
{
	int a;

	a = 0;
}

void varv(__va_list ap)
{
	int c;
	c = __va_arg ( ap ,c ) ;
}

__va_list x;

void var(char *fmt, ...)
{
	__va_list ap;

	__va_start( ap ,fmt ) ;
	varv(ap);
	__va_end ( ap );
}

void main(void)
{
	var('c');
}
