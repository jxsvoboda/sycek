/*
 * Variable arguments
 */

__va_list vl;
__va_list vl2;
int s0, s1, s2, s3, s4;
int d0, d1, d2, d3, d4;

void vastart(int a0, ...)
{
	d0 = a0;
	__va_start(vl, a0);
}

void vacopy(int a0, ...)
{
	d0 = a0;
	__va_start(vl, a0);
	__va_copy(vl2, vl);
}

void vaend(int a0, ...)
{
	d0 = a0;
	__va_start(vl, a0);
	__va_end(vl);
}

void var1(int a0, ...)
{
	d0 = a0;
	__va_start(vl, a0);
	d1 = __va_arg(vl, int);
	d2 = __va_arg(vl, int);
	d3 = __va_arg(vl, int);
	d4 = __va_arg(vl, int);
}

void cvar1(void)
{
	var1(s0, s1, s2, s3, s4);
}

void var2(int a0, int a1, ...)
{
	d0 = a0;
	d1 = a1;
	__va_start(vl, a1);
	d2 = __va_arg(vl, int);
	d3 = __va_arg(vl, int);
	d4 = __va_arg(vl, int);
	__va_end(vl);
}

void var3(int a0, int a1, int a2, ...)
{
	d0 = a0;
	d1 = a1;
	d2 = a2;
	__va_start(vl, a2);
	d3 = __va_arg(vl, int);
	d4 = __va_arg(vl, int);
	__va_end(vl);
}

void var4(int a0, int a1, int a2, int a3, ...)
{
	d0 = a0;
	d1 = a1;
	d2 = a2;
	d3 = a3;
	__va_start(vl, a3);
	d4 = __va_arg(vl, int);
	__va_end(vl);
}

void varvap(int a0, __va_list ap)
{
	d0 = a0;
	d1 = __va_arg(ap, int);
	d2 = __va_arg(ap, int);
	d3 = __va_arg(ap, int);
	d4 = __va_arg(ap, int);
}

void varv(int a0, ...)
{
	__va_start(vl, a0);
	varvap(a0, vl);
}

void cvarv(void)
{
	varv(s0, s1, s2, s3, s4);
}

long sl0, sl1, sl2;
long l0, l1, l2;

/*
 * a0 is passed in HL, DE. a1 does not fit into registers entirely.
 * Since varl is variadic, it must be passed entirely on the stack,
 * unlike with a regular function, where 2 bytes would be passed in
 * BC and two bytes on the stack.
 */
void varl(long a0, long a1, ...)
{
	l0 = a0;
	l1 = a1;

	__va_start(vl, a1);
	l2 = __va_arg(vl, long);
}

void cvarl(void)
{
	varl(sl0, sl1, sl2);
}
