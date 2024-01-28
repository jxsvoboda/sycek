/*
 * Implicit pointer conversion
 */

int *ip;
void *vp;

void ptr_to_void_ptr(void)
{
	/* Any pointer type can be implicitly converted to void pointer */
	vp = ip;
}
