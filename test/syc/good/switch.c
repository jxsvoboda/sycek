/*
 * Switch statement
 */

int a;
int unr;
int c1;
int c2;
int c3;
int cdef;

void stswitch(void)
{
	switch (a) {
		/*
		 * Unreachable code that should be, nevertheless, generated
		 * Let's not create a special diagnostic for this case,
		 * instead it can be caught by a general unreachable code
		 * test.
		 */
		unr = 1;
	case 1:
		c1 = 1;
	case 2:
		c2 = 1;
	default:
		cdef = 1;
	case 3:
		c3 = 1;
	}
}
