/*
 * Local symbol test. The local symbol 'i' is defined in both modules.
 * The linker needs to tell them apart correctly.
 */

static int i = 1;

int a(void)
{
	return i;
}
