/*
 * Suspicious bitwise operation involving truth values
 */

int c;

void truthband(void)
{
	c = (int)((0 < 1) & 1);
	c = (int)(1 & (0 < 1));
	c = (int)((0 < 1) & (0 < 1));
}

void truthbor(void)
{
	c = (int)((0 < 1) | 1);
	c = (int)(1 | (0 < 1));
	c = (int)((0 < 1) | (0 < 1));
}

void truthbxor(void)
{
	c = (int)((0 < 1) ^ 1);
	c = (int)(1 ^ (0 < 1));
	c = (int)((0 < 1) ^ (0 < 1));
}

void truthbnot(void)
{
	c = (int)~(0 < 1);
}

/* TODO: Compound assignments once we have _Bool */
