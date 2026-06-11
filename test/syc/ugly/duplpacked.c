/*
 * Duplicate attribute 'packed'.
 */

typedef struct s {
} __attribute__((packed,packed)) __attribute__((packed)) mystruct_t;
