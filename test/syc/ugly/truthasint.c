/*
 * Truth value used as an integer
 */

int a;
int b;
int c;

/*
 * Truth value assigned to integer variable.
 * Check that all relevant operators produce a truth value.
 */
void convint(void)
{
	c = !(a < b);
	c = a < b;
	c = a <= b;
	c = a == b;
	c = a != b;
	c = a >= b;
	c = (a < b) || (a > b);
	c = (a < b) && (a > b);
}

/* Declare array whose dimension is a truth value */
int d[1 < 0];
