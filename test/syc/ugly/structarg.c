/*
 * Definition of 'struct foo' inside parameter list will not be visible outside of function declaration/definition.
 *
 * We also verify that 'struct foo' defined inside parameter list
 * really does not collide with 'struct foo' defined in global scope.
 */

void fun(struct foo { char a; } f);
struct foo {
	int x;
} f;
