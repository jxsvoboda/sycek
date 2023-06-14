/*
 * Struct/union/enum definition inside a cast.
 */

void main(void)
{
	void *p;

	p = (void *)(struct foo { int x; } *)&p;
}
