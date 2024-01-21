/*
 * Conditional operator with two distinct enum types.
 */

enum f {
	f1 = 0x1,
	f2 = 0x2,
	f3 = 0x3
};

enum g {
	g1,
	g2
};

enum f d;
enum f a;
enum g b;
int c;

void main(void)
{
	d = (c != 0) ? a : b;
}
