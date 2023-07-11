/*
 * Negative number converted to unsigned before comparison
 */

int res;

unsigned u;

void lt(void)
{
	res = (int)(u < -1);
}

void leq(void)
{
	res = (int)(u <= -1);
}

void gt(void)
{
	res = (int)(u > -1);
}

void geq(void)
{
	res = (int)(u >= -1);
}

void eq(void)
{
	res = (int)(u == -1);
}

void neq(void)
{
	res = (int)(u != -1);
}
