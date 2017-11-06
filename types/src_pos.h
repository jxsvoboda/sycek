/*
 * Source code position
 */

#ifndef TYPES_SRC_POS_H
#define TYPES_SRC_POS_H

#include <stddef.h>

enum {
	src_pos_fname_max = 16
};

/** Source code position */
typedef struct {
	char file[src_pos_fname_max];
	size_t line;
	size_t col;
} src_pos_t;

#endif
