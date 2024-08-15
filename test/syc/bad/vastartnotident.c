/*
 * Use of __va_start with last parameter not being identifier
 */

__va_list ap;

void var(int a, ...)
{
	__va_start(ap, 1);
}
