/*
 * Switch statement
 */

int a;

int stswitch(void)
{
	switch (a) {
		/*
		 * Unreachable code that should be, nevertheless, generated
		 * Let's not create a special diagnostic for this case,
		 * instead it can be caught by a general unreachable code
		 * test.
		 */
		return 1;
	case 1:
		return 10;
	case 2:
		return 20;
	}

	return 0;
}
