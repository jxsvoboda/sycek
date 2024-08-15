/*
 * Use of __va_start in a function that does not take variable arguments.
 */

__va_list ap;

void var(char *fmt)
{
	__va_start(ap, fmt);
}
