/*
 * Greater than
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int gt_char(void)
{
	return (int)(ca > cb);
}

int gt(void)
{
	return (int)(a > b);
}

int gt_long(void)
{
	return (int)(la > lb);
}

int gt_longlong(void)
{
	return (int)(lla > llb);
}
