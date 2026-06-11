/*
 * Attribute 'packed' should not have any arguments.
 */

typedef struct s {
} __attribute__((packed(1))) mystruct_t;
