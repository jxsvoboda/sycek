/*
 * Struct/union/enum definition inside a cast.
 */

void main(void)
{
	(void)(enum foo { e1, e2 }) e1;
}
