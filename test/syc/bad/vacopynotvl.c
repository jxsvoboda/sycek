/*
 * Expression of type __va_list expected.
 */

int a, b;

void var(char *fmt, ...)
{
	__va_copy(a, b);
}
