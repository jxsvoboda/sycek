/*
 * Local variables
 */

char sca, scb;
char dca, dcb;

int sa, sb;
int da, db;

long sla, slb;
long dla, dlb;

long long slla, sllb;
long long dlla, dllb;

void lvars_char(void)
{
	char ca, cb;

	ca = sca;
	cb = scb;

	dca = ca;
	dcb = cb;
}

void lvars(void)
{
	int a, b;

	a = sa;
	b = sb;

	da = a;
	db = b;
}

void lvars_long(void)
{
	long la, lb;

	la = sla;
	lb = slb;

	dla = la;
	dlb = lb;
}

void lvars_longlong(void)
{
	long long lla, llb;

	lla = slla;
	llb = sllb;

	dlla = lla;
	dllb = llb;
}
