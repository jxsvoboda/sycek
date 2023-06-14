/*
 * Struct/union/enum definition inside a cast.
 */

void main(void)
{
	void *p;

	p = (void *)(union foo { int x; } *)&p;
}
