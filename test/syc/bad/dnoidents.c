/*
 * Declarator expected before ','.
 *
 * The parser will happily chew on this, producing an ident-declarator
 * list with to dnoidents. We need to explicitly check for this in
 * code generator. One dnodident will produce a warning (useless type
 * in empty declaration). If it is followed by another entry, it
 * is an error.
 */

typedef struct {
} ,;
