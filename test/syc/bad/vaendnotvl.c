/*
 * Expression of type __va_list expected.
 */

int ap;

void var(char *fmt, ...)
{
	__va_end(ap);
}
