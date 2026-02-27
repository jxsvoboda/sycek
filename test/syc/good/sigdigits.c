/*
 * Do NOT warn that conversion may loose significant digits.
 *
 * Here are the cases where we can be smart and not warn about conversion
 * loosing significant digits. (Thus no need for explicit type cast.)
 *
 *   a) we can prove that there are less significant bits
 *      (bitwise and, division, shift right, converted from smaller type)
 *
 *   b) if there was no promotion, the value would fit (unpromoted bits).
 */

unsigned long long ull;
unsigned long ul;
unsigned u;
unsigned char uc;
long long ll;
long l;
int i;
char c;

enum {
	e1 = 1
};

void foo(void)
{
	/* Dividing by number >= 4G removes 32 sigificant bits. */
	ul = ull / 0x100000000ull;

	/* Dividing by number >= 64k removes 16 sigificant bits. */
	u = ul / 65536ul;

	/* Dividing by number >= 256 removes 8 sigificant bits. */
	uc = u / 256;

	/* Dividing by number >= 4G removes 32 sigificant bits. */
	l = ll / 0x100000000ll;

	/* Dividing by number >= 64k removes 16 sigificant bits. */
	i = l / 65536l;

	/* Dividing by number <= -64k removes 16 sigificant bits. */
	i = l / -65536l;

	/* Dividing by number >= 256 removes 8 sigificant bits. */
	c = i / 256;

	/* Shifting right removes 32 significant bits. */
	ul = ull >> 32;

	/* Shifting right removes 16 significant bits. */
	u = ul >> 16;

	/* Shifting right removes 8 significant bits. */
	uc = u >> 8;

	/* Upconverted from char, still just 8 significant bits. */
	c = (unsigned)c;

	/* Shifting right removes 32 significant bits. */
	l = ll >> 32;

	/* Shifting right removes 16 significant bits. */
	i = l >> 16;

	/* Shifing right removes 8 significant bits. */
	c = i >> 8;

	/* Masking with small enough type. */
	ul = ull & ul;

	/* Masking with small enough type. */
	u = ul & u;

	/* Masking with small enough type. */
	uc = u & uc;

	/* Masking with small enough constant. */
	ul = ull & 0xfffffffful;

	/* Masking with small enough constant. */
	u = ul & 0xffffu;

	/* Masking with small enough constant. */
	c = u & 0xff;

	/* Unpromoted size fits. */
	c = c + c;

	/* Unpromoted size fits. */
	c = c * c;

	/* Unpromoted size fits. */
	c = c - c;

	/* Unpromoted size fits. */
	c = (1 < 0) ? c : c;

	/* Enum value and constant are both small enough to fit into char. */
	c = (1 < 0) ? e1 : 0;
}
