/*
 * Preprocessor directives should not have any indentation
 */

#include <stdio.h>

#ifndef FOO
    #define FOO
    #include <foostuff.h>
#endif

#ifndef BAR
	#define BAR
#endif
