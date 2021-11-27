/*
 * Superfluous parentheses around function identifier
 * or function type declarator is missing '*'
 */

/* Superfluous parentheses in function definition */
int (a)(void)
{
	return 0;
}

/* Superfluous parentheses in function declaration */
int (b)(void);

/* Function type declaration is missing '*' */
typedef int (c)(void);

/* Parentheses around function pointer are okay */
int (*d)(void);
