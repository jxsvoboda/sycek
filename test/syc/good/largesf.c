/*
 * Large stack frame (virtual registers not within the reach of IX+d where
 * IX == fp).
 */

long long v1;
long long v2;
long long v3;
long long v4;
long long v5;
long long v6;
long long v7;
long long v8;
long long v9;
long long v10;
long long v11;
long long v12;
long long v13;
char v14;

int ba1;
int ba2;
int ba3;
int ba4;

void bar(int a1, int a2, int a3, int a4)
{
	ba1 = a1;
	ba2 = a2;
	ba3 = a3;
	ba4 = a4;
}

void foo(void)
{
	/*
	 * Fill up first 128 bytes worth of virtual register stack space.
	 * Note that this only works without ANY optimizations. With the
	 * last few operations we should need to adjust IX to reach those
	 * virtual registers.
	 */
	v1 = 1ll;
	v2 = 2ll;
	v3 = 3ll;
	v4 = 4ll;
	v5 = 5ll;
	v6 = 6ll;
	v7 = 7ll;
	v8 = 8ll;
	v9 = 9ll;
	v10 = 10ll;
	v11 = 11ll;
	v12 = 12ll;
	v13 = 13ll;

	/* At this point we should be already adjusting IX */
	v14 = 14;

	/*
	 * IX is computed from SP. Now let's test if we account for SP
	 * moving during the course of the procedure. This hapenns if we
	 * PUSH function arguments to the stack. Three 16-bit arguments
	 * are passed via registers. The fourth is pushed on the stack,
	 * which means the register allocator must compensate the IX
	 * adjustment values accordingly until SP is restored.
	 */
	bar(10, 20, 30, 40);
}
