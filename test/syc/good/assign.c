/*
 * Example file for compilation with Syc.
 */

char ca, cb = 1;
int a, b = 1, c = 2;
long la, lb = 1;
long long lla, llb = 1;

void assign_var_char(void)
{
	ca = cb;
}

int assign_var(void)
{
	/* Variable assignment */
	a = b = 3;

	/* Read variable to verify value has been written properly */
	return a;
}

void assign_var_long(void)
{
	la = lb;
}

void assign_var_longlong(void)
{
	lla = llb;
}
