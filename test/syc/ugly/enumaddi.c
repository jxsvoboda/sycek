/*
 * Enum should be the left operand while adjusting.
 */
enum e {
	e1, e2
};

enum e x;

void foo(void)
{
	x = e1;
	x = 1 + x;
}
