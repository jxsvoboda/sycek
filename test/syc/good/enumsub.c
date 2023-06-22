/*
 * Subtrac int from enum
 */


enum e {
    e1,
    e2,
    e3,
    e4,
    e5
};

enum e x;
enum e y;

void esub(void)
{
	int z;

	x = e5;
	--x;
	x--;
	x = x - 1;
	x -= 1;
	z = x - y;
}
