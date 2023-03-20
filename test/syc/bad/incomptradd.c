/*
 * Indexing pointer to incomplete type
 */

struct foo *fp;

void f(void)
{
	++fp;
}
