/*
 * Test supported C syntax extensions via use of C preprocessor
 */

typedef struct {
	/* Macro that declares a struct member */
	FIELD_DECL(x, 1);
} foo_t;

/* Macro that declares a global object */
GLOBAL_DECL(foo);

/* Macro that declares a global object with non-trivial arguments */
GLOBAL_DECL_2(bar, 1, 'x', FOO(x));

/* Macro that stands as the header of a function definition */
MY_FUNCTION_DECL(fun)
{
	return 0;
}

/* Symbolic variables used as type specifiers/qualifiers */
MY_UNSIGNED MY_LONG MY_SOMETHING foo_t foo;
MY_UNSIGNED MY_LONG int MY_SOMETHING bar;

/*
 * Symbolic variable or macro used as an attribute at the end of a function
 * declaration header
 */
int my_fun(void)
    __attribute__((noreturn))
    MY_ATTR MY_SPEC(2, 1);

int main(void)
{
	/*
	 * Symbolic variables and macros as elements in multipart string
	 * literal
	 */
	printf("Hello " VARIABLE " world " MACRO(foo, 1, "x") "!\n");

	/* Macro that takes a type name as argument (not an expression) */
	printf("%d", MY_SIZEOF(int *));
	printf("%d", MY_SIZEOF(foo_t []));
	printf("%d", MY_SIZEOF(int (*foo)(int, int)));

	/* Loop macro */
	MY_FOR_EACH(a, b, c) {
		do_something();
	}
}
