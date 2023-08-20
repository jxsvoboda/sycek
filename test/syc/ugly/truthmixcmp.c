/*
 * Comparison of truth value with integer.
 * Note that comparing a truth value with a truth value is okay.
 */

int c;

void truthmixlt(void)
{
	c = (int)((0 < 1) < 1);
	c = (int)(1 < (0 < 1));
}

void truthmixlte(void)
{
	c = (int)((0 < 1) <= 1);
	c = (int)(1 <= (0 < 1));
}

void truthmixgt(void)
{
	c = (int)((0 < 1) > 1);
	c = (int)(1 > (0 < 1));
}

void truthmixgte(void)
{
	c = (int)((0 < 1) >= 1);
	c = (int)(1 >= (0 < 1));
}

void turhtmixeq(void)
{
	c = (int)((0 < 1) == 1);
	c = (int)(1 == (0 < 1));
}

void truthmixneq(void)
{
	c = (int)((0 < 1) != 1);
	c = (int)(1 != (0 < 1));
}
