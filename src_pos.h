/*
 * Source code position
 */

#ifndef SRC_POS_H
#define SRC_POS_H

#include <stddef.h>
#include <stdio.h>
#include <types/src_pos.h>

extern int src_pos_print_range(src_pos_t *, src_pos_t *, FILE *);
extern void src_pos_fwd_char(src_pos_t *, char);
extern void src_pos_set(src_pos_t *, const char *, size_t, size_t);

#endif
