/*
 * Truth value comparison
 */

int c;

void truthcmp(void)
{
	c = (int)((0 < 1) < (0 < 1));
	c = (int)((0 < 1) <= (0 < 1));
	c = (int)((0 < 1) > (0 < 1));
	c = (int)((0 < 1) >= (0 < 1));
	c = (int)((0 < 1) == (0 < 1));
	c = (int)((0 < 1) != (0 < 1));
}
