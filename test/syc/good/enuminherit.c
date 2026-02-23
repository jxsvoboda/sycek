/*
 * Non-strict enum constant inheriting strict enum type.
 */

/* A named/strict enumerated type. */
enum e1 {
	e1_a,
	e1_b,
	e1_c
};

/*
 * Now we'd like a constant that defines the upper bound of e1.
 * We do not want this to be part of e1, because we do not want the user
 * to handle it in switch() statements.
 */
enum {
	/*
	 * The enum being unnamed/non-strict this will inherit the type
	 * of the initializer expression, which is enum e1.
	 */
	e1_lim = e1_c + 1
};

/* Type of array dimension is enum e1. */
int arr[e1_lim];

void foo(void)
{
	enum e1 x;

	/* e1_lim is of type enum e1 so this will work just fine. */
	for (x = e1_a; x < e1_lim; x++)
		arr[x] = 0;
}

void bar(enum e1 x)
{
	switch (x) {
	case e1_a:
		break;
	case e1_b:
		break;
	case e1_c:
		break;
		/*
		 * note e1_lim is of type enum e1, but not part of it
		 * so no need to handle it here.
		 */
	}
}
