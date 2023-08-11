/*
 * Return pointer from function
 */

char *p;
char *r;

/* Function returning pointer */
char *ret_ptr(void)
{
	return p;
}

/* Call function returning pointer */
void call_ptr(void)
{
	r = ret_ptr();
}
