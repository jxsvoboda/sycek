/*
 * Return different-size types from function
 */

char c;
char rc;
int i;
int ri;
long l;
long rl;
long long ll;
long long rll;

/* Function returning char */
char ret_char(void)
{
	return c;
}

/* Call function returning char */
void call_char(void)
{
	rc = ret_char();
}

/* Function returning integer */
int ret_int(void)
{
	return i;
}

/* Call function returning integer */
void call_int(void)
{
	ri = ret_int();
}

/* Function returning long */
long ret_long(void)
{
	return l;
}

/* Call function returning long */
void call_long(void)
{
	rl = ret_long();
}

/* Function returning long long */
long long ret_longlong(void)
{
	return ll;
}

/* Call function returning long long */
void call_longlong(void)
{
	rll = ret_longlong();
}
