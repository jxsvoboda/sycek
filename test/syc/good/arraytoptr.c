/*
 * Converting array to pointer
 */

int a[10];
int *p;

/* Convert array to pointer by means of implicit cast */
void cast_array_ptr(void)
{
	p = a;
}

/*
 * Convert array to pointer using the plus operator.
 * (The compiler treats this as a special case)
 */
void index_array_ptr(void)
{
	p = a + 1;
}

/* Convert pointer to array while initializing global variable */
int *q = a;

/* Convert pointer to array while initializing local variable */
void cast_init(void)
{
	int *pp = a;
	p = pp;
}

/* Convert pointer to array while returning value from function */
int *cast_return(void)
{
	return a;
}

void fun(int *pp)
{
	p = pp;
}

/* Convert pointer to array while passing argument to function */
void cast_pass(void)
{
	fun(a);
}
