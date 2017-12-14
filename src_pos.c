/*
 * Source code position
 */

#include <merrno.h>
#include <src_pos.h>
#include <stdio.h>

enum {
	tab_width = 8
};

/** Print position range.
 *
 * @param bpos Position of the beginning of range
 * @param epos Position of the end of the range (inclusive)
 * @param f Output file
 *
 * @return EOK on success, EIO on I.O error
 */
int src_pos_print_range(src_pos_t *bpos, src_pos_t *epos, FILE *f)
{
	if (bpos->line == epos->line && bpos->col == epos->col) {
		if (fprintf(f, "file:%zu:%zu", bpos->line, bpos->col) < 0)
			return EIO;
	} else if (bpos->line == epos->line) {
		if (fprintf(f, "file:%zu:%zu-%zu", bpos->line, bpos->col,
		    epos->col) < 0)
			return EIO;
	} else {
		if (fprintf(f, "file:%zu:%zu-%zu:%zu", bpos->line, bpos->col,
		    epos->line, epos->col) < 0)
			return EIO;
	}

	return EOK;
}

/** Set source position.
 *
 * @param pos Position structure to initialize
 * @param fname File name
 * @param line Line number (starting from 1)
 * @param col Column number (starting from 1)
 */
void src_pos_set(src_pos_t *pos, const char *fname, size_t line, size_t col)
{
	snprintf(pos->file, src_pos_fname_max, fname);
	pos->line = line;
	pos->col = col;
}

/** Move position one character forward based on character.
 *
 * @param pos Current position to be modified
 * @param c Character at current position
 */
void src_pos_fwd_char(src_pos_t *pos, char c)
{
	switch (c) {
	case '\n':
		++pos->line;
		pos->col = 1;
		break;
	case '\t':
		/* Move to the next multiple of tab width */
		pos->col += tab_width - (pos->col - 1) % tab_width;
		break;
	default:
		++pos->col;
	}
}
