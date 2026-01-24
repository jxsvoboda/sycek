/*
 * Declaration specifier order.
 *
 * For a unified coding style the declaration specifiers should come in
 * a specific order. The C language allows the specifiers come in any,
 * sometimes truly absurd and confusing, order. Most C programs tend to
 * always follow the same order, though.
 *
 * Here we invert the order and see if we get the correct warnings.
 */

/* Basic type specifiers should be in order: sign, length, type */
int long unsigned a;

/* Type qualifiers should be in order: const, restrict, volatile, (atomic?) */
int *volatile restrict const b;

/*
 * Declaration specifiers should be in order:
 *   storage class, type qualifier, inline, attribute, type specifier
 */
int __attribute__((foo)) inline const static c;

/* A remarkably strange example of 'storage class' coming after type specifier */
int typedef foo_t;
