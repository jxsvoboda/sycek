/*
 * Invalid use of 'inline' inside __va_arg().
 */

void var1(int a0, ...)
{
	__va_list vl;
	int d1;

	__va_start(vl, a0);
	d1 = __va_arg(vl, inline int);

	(void)a0;
}
