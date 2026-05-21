/*
 * Inline function declaration, definition
 */

/* External linkage, this is an inline definition (not exported) */
inline void finline(void)
{
}

/* Internal linkage */
static inline void fsinline(void)
{
}

/* External linkage, not an inline definition (exported). */
extern inline void fxinline(void)
{
}

/* Static inline function can be delared and not defined. */
static inline void finlinedecl(void);