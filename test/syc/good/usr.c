/*
 * User service routine
 */

/* External user service routine declaration */
int xusr(void) __attribute__((usr));

/* External function declaration */
int xnormal(void);

/* User service routine definition */
int husr(void) __attribute__((usr))
{
	return 6;
}

/* Normal function definition */
int hnormal(void)
{
	return 6;
}

/* Call user service routine */
int call_usr(void)
{
	return husr();
}

/* Call normal function */
int call_normal(void)
{
	return hnormal();
}

/* Call external user service routine */
int call_xusr(void)
{
	return xusr();
}

/* Call normal external function */
int call_xnormal(void)
{
	return xnormal();
}
