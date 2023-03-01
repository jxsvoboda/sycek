/*
 * Type definition in a  non-global scope.
 * Declaration of 'foo' shadows a wider-scope declaration.
 * 'foo' is defined, but not used.
 */

typedef long **intptr_t, foo;

int bar(void)
{
	typedef long foo;
}
