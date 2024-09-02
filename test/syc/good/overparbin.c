/*
 * Over-parenthesized binary operator '+', '-' or '*'.
 *
 * Because the parser knows nothing about semantics, it can misparse
 * overparemthesized addition, subtraction or multiplication as
 * a cast operator followed by unary '+' sign, '-' sign or dereference '*'.
 *
 * The code generator needs to handle this as a special case.
 */

int overpar_mul(int link)
{
	return (int)((link) *(0));
}

int overpar_add(int link)
{
	return (int)((link) +(0));
}

int overpar_sub(int link)
{
	return (int)((link) -(0));
}

int x, y, z;

void foo(int a, int b, int c)
{
	x = a;
	y = b;
	z = c;
}

void overpar_call(void)
{
	(foo)(1, 2, 3);
}
