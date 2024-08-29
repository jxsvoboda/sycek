/*
 * Constant expression - multiplication
 */

char cs = 0xa * 0x4; /* 0x28 */
unsigned char cu = (unsigned char)(0x3au * 0xebu); /* 0x3e */
int s = 0x3a * 0xeb; /* 0x353e */
unsigned u = 0xabe1u * 0x57a2u; /* 0x3b62 */
long ls = 0xabe1l * 0x57a2l; /* 0x3ad63b62 */
unsigned long lu = 0xd9240037ul * 0x2fb19151ul; /* 0x578a3867 */
long long lls = 0xd9240037ll * 0x2fb19151ll; /* 0x28743930578a3867 */
unsigned long long llu = 0x962267b3ca0c043cull * 0xb56df02ca59497b4ull; /* 0xe81be7e9445a25e30 */
