/*
 * Use of __va_start in function that has no fixed parameters.
 */

__va_list ap;

void var(...)
{
	__va_start(ap, a);
}
