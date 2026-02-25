/*
 * Enum type
 *
 * For an anonymous enum we choose to give each member the same type
 * as its initializer. Verify the correct type using sizeof.
 * Also verify that the value is generated correctly.
 */

enum {
	ei = 0x7fff,
	el = 0x7fffffffl,
	ell = 0x7fffffffffffffffll
};

unsigned si = sizeof(ei);
unsigned sl = sizeof(el);
unsigned sll = sizeof(ell);

int i;
long l;
long long ll;

void fi(void)
{
	i = ei;
}

void fl(void)
{
	l = el;
}

void fll(void)
{
	ll = ell;
}
