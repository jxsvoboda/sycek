/*
 * Pointer declaration
 */

int i;

/* Pointer declared as a global variable */
int *ip, *ip2;

/* Function declaration with pointer argument */
void funptr(int *x);

/* Function declaration with pointer argument, no identifier */
void funptr2(int *);

/* Function definition with pointer argument */
void funptrdef(int *ptrarg)
{
	ip = ptrarg;
}

void ptrlocal(void)
{
	/* Pointer declared as local variable */
	int *iplocal;

	iplocal = ip;
	ip2 = iplocal;
}
