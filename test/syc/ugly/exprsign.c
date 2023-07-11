/*
 * Conversion from int to unsigned changes signedness
 */

int i;
unsigned u;

void foo(void)
{
	u = i;
}
