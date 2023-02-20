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

/* 4096 in decimal */
unsigned decval_u = 4096u;
/* 4096 in octal */
unsigned octval_u = 010000u;
/* 4096 in hex */
unsigned hexval_u = 0x1000u;
/* 43981 in hex */
unsigned hexabcd_u = 0xabcdu;
/* 43981 in hex */
unsigned hexABCD_u = 0xABCDu;

/* 16777216L in decimal */
long decval_l = 16777216L;
/* 16777216L in octal */
long octval_l = 0100000000L;
/* 16777216L in hex */
long hexval_l = 0x1000000L;
/* 0xabcddcbaL in hex */
long hexabcddcba_l = 0xabcddcbaL;
/* 0xABCDDCBAl in hex */
long hexABCDDCBA_l = 0xABCDDCBAl;

/* 16777216UL in decimal */
unsigned long decval_ul = 16777216UL;
/* 16777216UL in octal */
unsigned long octval_ul = 0100000000UL;
/* 16777216UL in hex */
unsigned long hexval_ul = 0x1000000UL;
/* 0xabcddcbaUL in hex */
unsigned long hexabcddcba_ul = 0xabcddcbaUL;
/* 0xABCDDCBAul in hex */
unsigned long hexABCDDCBA_ul = 0xABCDDCBAul;

/* 2**48 in decimal */
long long decval_ll = 281474976710656LL;
/* 2**48 in octal */
long long octval_ll = 010000000000000000LL;
/* 2**48 in hex */
long long hexval_ll = 0x1000000000000LL;
/* 0xabcddcbaL in hex */
long long hexabcddcba_ll = 0xabcddcbaabcddcbaLL;
/* 0xABCDDCBAl in hex */
long long hexABCDDCBA_ll = 0xABCDDCBAABCDDCBAll;

/* 2**48 in decimal */
unsigned long long decval_ull = 281474976710656ULL;
/* 2**48 in octal */
unsigned long long octval_ull = 010000000000000000ULL;
/* 2**48 in hex */
unsigned long long hexval_ull = 0x1000000000000ULL;
/* 0xabcddcbaL in hex */
unsigned long long hexabcddcba_ull = 0xabcddcbaabcddcbaULL;
/* 0xABCDDCBAl in hex */
unsigned long long hexABCDDCBA_ull = 0xABCDDCBAABCDDCBAull;

int i;
unsigned u;
long l;
unsigned long ul;
long long ll;
unsigned long long ull;

void lit_int(void)
{
	/* Integer literal expression */
	i = 0x1234;
}

void lit_uint(void)
{
	/* Unsigned integer literal expression */
	u = 0x1234u;
}

void lit_long(void)
{
	/* Long integer literal expression */
	l = 0x12345678l;
}

void lit_ulong(void)
{
	/* Unsigned long integer literal expression */
	ul = 0x12345678ul;
}

void lit_longlong(void)
{
	/* Long long integer literal expression */
	ll = 0x1234567812345678ll;
}

void lit_ulonglong(void)
{
	/* Long long integer literal expression */
	ull = 0x1234567812345678ull;
}
