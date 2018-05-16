/* Make sure this is not misinterpreted as sizeof ((int)(*2)) */
int a = sizeof(int) * 2;

/* array[0] must be parsed as an expression, not a type name */
int b = sizeof(array) / sizeof(array[0]);

/* Expression */
int c = sizeof(a * b);

/* Type name */
int d = sizeof(foo_t *);

int main(void)
{
	/* Break allowed after '[' */
	int i = foo[
	    bar];

	/* Break allowed after '(' */
	int i = fun(
	    bar);

	/* Break allowed after '->' */
	int i = ptr->
	    member;

	/* Break allowed after '.' */
	int i = rec.
	    member;

	/*
	 * Expression statements vs. declaration statements
	 */

	*a = b;
	int *c = d;
	t_t *e = f;
}
