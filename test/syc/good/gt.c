/*
 * Greater than
 */

int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void gt_char(void)
{
	res = (int)(ca > cb);
}

void gt(void)
{
	res = (int)(a > b);
}

void gt_long(void)
{
	res = (int)(la > lb);
}

void gt_longlong(void)
{
	res = (int)(lla > llb);
}
