/*
 * Add int to enum
 */


enum e {
    e1,
    e2,
    e3,
    e4,
    e5
};

enum e x;

void eadd(void)
{
	x = e1;
	++x;
	x++;
	x = x + 1;
	x += 1;
}
