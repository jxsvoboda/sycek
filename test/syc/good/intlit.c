/*
 * Integer constants as initializers and literals
 */

/* 4096 in decimal */
int decval = 4096;
/* 4096 in octal */
int octval = 010000;
/* 4096 in hex */
int hexval = 0x1000;
/* 43981 in hex */
int hexabcd = 0xabcd;
/* 43981 in hex */
int hexABCD = 0xABCD;

/* 1677216L in decimal */
long decval_l = 1677216L;
/* 1677216L in octal */
long octval_l = 0100000000L;
/* 1677216L in hex */
long hexval_l = 0x1000000L;
/* 0xabcddcbaL in hex */
long hexabcddcba_l = 0xabcddcbaL;
/* 0xABCDDCBAl in hex */
long hexABCDDCBA_l = 0xABCDDCBAl;

/* 2**48 in decimal */
long long decval_ll = 281474976710656LL;
/* 2**48 in octal */
long long octval_ll = 0100000000000000000LL;
/* 2**47 in hex */
long long hexval_ll = 0x1000000000000LL;
/* 0xabcddcbaL in hex */
long long hexabcddcba_ll = 0xabcddcbaabcddcbaLL;
/* 0xABCDDCBAl in hex */
long long hexABCDDCBA_ll = 0xABCDDCBAABCDDCBAll;

int a;
long b;
long long c;

void lit_int(void)
{
	/* Integer literal expression */
	a = 0x1234;
}

void lit_long(void)
{
	/* Long integer literal expression */
	b = 0x12345678l;
}

void lit_longlong(void)
{
	/* Long long integer literal expression */
	c = 0x1234567812345678ll;
}
