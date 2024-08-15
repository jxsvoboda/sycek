/*
 * Use of __va_start with last parameter not being identifier of the last
 * fixed parameter
 */

__va_list ap;

void var(int a, int b, ...)
{
	__va_start(ap, a);
}
