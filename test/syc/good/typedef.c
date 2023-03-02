/*
 * Type definition
 */

typedef long **pplong_t, *plong_t, long_t;
typedef int int_t;

int_t test_int_t(int_t x, int_t y)
{
	return x + y;
}

long_t l;
plong_t pl;
pplong_t ppl;

void test(void)
{
	pl = &l;
	ppl = &pl;
}
