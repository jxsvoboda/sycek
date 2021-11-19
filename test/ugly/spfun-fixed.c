/*
 * Superfluous parentheses around function identifier
 */

/* Superfluous parentheses in function definition */
int a(void)
{
	return 0;
}

/* Superfluous parentheses in function declaration */
int b(void);

/* Superfluous parenteses in function-type definition */
typedef int c(void);

/* Parenteses around function pointer are okay */
int (*d)(void);
