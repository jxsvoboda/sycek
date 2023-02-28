/*
 * Pointer subscripting
 */

int *p;
char cidx;
int idx;
int i0;
int i;

/* Pointer subscripted by char index */
void ptrsubs_char(void)
{
	i = p[cidx];
}

/* Pointer subscripted by int index */
void ptrsubs_int(void)
{
	i = p[idx];
}
