/*
 * An inline definition of afunction with external linkage shall
 * not contain a reference to an identifier with internal linkage.
 */

static int i;

inline void finline(void)
{
	(void)i;
}
