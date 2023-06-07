/*
 * '->' requires a pointer to a struct or union.
 */

int i;
int *p;

void set_f(void)
{
	p = &i;

	p->x = 1;
}
