/*
 * Expression of type __va_list expected.
 */

int ap;
int i;

void var(char *fmt, ...)
{
	i = __va_arg(ap, int);
}
