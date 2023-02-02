/*
 * Return different-size types from function
 */

char a;
char rc;
int ri;
long rl;
long long rll;

/* Function returning char */
char retc(void)
{
	return a;
}

/* Call function returning char */
void call_c(void)
{
	rc = retc();
}

/* Function returning integer */
int reti(void)
{
	return 1;
}

/* Call function returning integer */
void call_i(void)
{
	ri = reti();
}

/* Function returning long */
long retl(void)
{
	return 1l;
}

/* Call function returning long */
void call_l(void)
{
	rl = retl();
}

/* Function returning long long */
long long retll(void)
{
	return 1ll;
}

/* Call function returning long long */
void call_ll(void)
{
	rll = retll();
}
