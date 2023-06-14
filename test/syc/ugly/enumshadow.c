/*
 * Definition of 'struct e' shadows a wider-scope struct, union or enum
 * definition.
 */

enum e {
	e1
};

void main(void)
{
	enum e {
		e2
	} x;

	x = e2;
}
