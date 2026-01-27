/*
 * Conditional operator + type qualifiers
 */

const int *pc;
volatile int *pv;

int c;

/*
 * Both operands are pointers. The result is a pointer to a type that
 * has qualifiers from both types.
 */
void cond_arr(void)
{
	pc = (c != 0 ? pc : pv);	/* discards volatile */
	pv = (c != 0 ? pc : pv);	/* discards const */
}
