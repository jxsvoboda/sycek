/*
 * If statement with integer parameters of different sizes.
 */

int d;

char c;
int i;
long l;
long long ll;

/* If statement with char argument. */
void if_char(void)
{
	if (c)
		d = 1;
	else
		d = 0;
}

/* If statement with int argument. */
void if_int(void)
{
	if (i)
		d = 1;
	else
		d = 0;
}

/* If statement with long argument. */
void if_long(void)
{
	if (l)
		d = 1;
	else
		d = 0;
}

/* If statement with long long argument. */
void if_longlong(void)
{
	if (ll)
		d = 1;
	else
		d = 0;
}
