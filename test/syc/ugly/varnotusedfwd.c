/*
 * Variable 'a' not used since forward declaration.
 */

int a;
int a = 1;


int b;

void fun(void)
{
	b = 1;
}

/* In this case, b is used, so no warning is generated */
int b = 2;
