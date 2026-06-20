/*
 * Local symbol test. The local symbol 'i' is defined in both modules.
 * The linker needs to tell them apart correctly.
 */

static int i = 2;

int a(void);

int b(void)
{
	int j;
	return i;
}

int resa;
int resb;

void run(void)
{
	/* This should be 1 */
	resa = a();
	/* This should be 2 */
	resb = b();
}
