/*
 * Declaration of 'e1' shadows a wider-scope declaration.
 * Definition of 'enum e' in a non-global scope.
 */

int e1;

void main(void)
{
	enum e {
		e1
	} x;

	x = e1;
}
