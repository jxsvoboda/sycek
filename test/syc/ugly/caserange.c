/*
 * Case expression value is out of range of type.
 */

char c;
unsigned char uc;
int i;
unsigned u;
long l;
unsigned long ul;
long long ll;
unsigned long long ull;

void inrange_c(void)
{
	switch (c) {
	case -128:
		break;
	case 0:
		break;
	case 127:
		break;
	}
}

void outofrange_c(void)
{
	switch (c) {
	case -129:
		break;
	case 128:
		break;
	}
}

void inrange_uc(void)
{
	switch (uc) {
	case 0:
		break;
	case 255:
		break;
	}
}

void outofrange_uc(void)
{
	switch (uc) {
	case -1:
		break;
	case 256:
		break;
	}
}

void inrange_i(void)
{
	switch (i) {
	case -32768l:
		break;
	case 0:
		break;
	case 32767:
		break;
	}
}

void outofrange_i(void)
{
	switch (i) {
	case -32769l:
		break;
	case 32768l:
		break;
	}
}

void inrange_u(void)
{
	switch (u) {
	case 0:
		break;
	case 65535u:
		break;
	}
}

void outofrange_u(void)
{
	switch (u) {
	case -1:
		break;
	case 65536l:
		break;
	}
}

void inrange_l(void)
{
	switch (l) {
	case -2147483648ll:
		break;
	case 0:
		break;
	case 2147483647l:
		break;
	}
}

void outofrange_l(void)
{
	switch (l) {
	case -2147483649ll:
		break;
	case 2147483648ll:
		break;
	}
}

void inrange_ul(void)
{
	switch (ul) {
	case 0:
		break;
	case 4294967295ul:
		break;
	}
}

void outofrange_ul(void)
{
	switch (ul) {
	case -1:
		break;
	case 4294967296ull:
		break;
	}
}

void inrange_ll(void)
{
	switch (ll) {
	case -2147483648ll:
		break;
	case 0:
		break;
	case 2147483647l:
		break;
	}
}

void outofrange_ll(void)
{
	switch (ll) {
	/* Cannot have signed constant >= 2^63 */
	case 9223372036854775808ull:
		break;
	}
}

void inrange_ull(void)
{
	switch (ull) {
	case 0:
		break;
	case 0xffffffffffffffffull:
		break;
	}
}

void outofrange_ull(void)
{
	switch (ull) {
	case -1:
		break;
	/* Cannot have constant >= 2^64 */
	}
}
