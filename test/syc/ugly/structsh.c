/*
 * Struct definition shadows a wider scope struct/union definition.
 */

struct foo {
	int x;
} f;

void fun(void)
{
	struct foo {
		int a;
	} g;
}
