/*
 * Logical AND
 */

int a, b;
int ea, eb;
int res;

int fa(void)
{
	ea = 1;
	return a;
}

int fb(void)
{
	eb = 1;
	return b;
}

void land(void)
{
	res = (int)(fa() && fb());
}
