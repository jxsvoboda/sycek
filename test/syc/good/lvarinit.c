/*
 * Local variable initialization
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void lvars_char(void)
{
	char c = ca;
	cb = c;
}

void lvars(void)
{
	int i = a;
	b = i;
}

void lvars_long(void)
{
	long l = la;
	lb = l;
}

void lvars_longlong(void)
{
	long long ll = lla;
	llb = ll;
}
