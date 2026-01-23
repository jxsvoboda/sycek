/*
 * Type specifier in type specifier list order.
 *
 * Here we invert the order and see if we get the correct warnings.
 */

/* Type qualifiers should be in order: const, restrict, volatile, (atomic?) */
int *volatile restrict const p;
