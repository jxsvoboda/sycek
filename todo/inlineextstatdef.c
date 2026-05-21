/*
 * An inline definition of a function with external linkage shall
 * not contain a definition of a modifiable object with static
 * storage duration.
 */

inline void finline(void)
{
	static int a;
}
