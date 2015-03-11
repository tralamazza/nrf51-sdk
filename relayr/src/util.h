#include <stdint.h>

#define ROUNDED_DIV(A, B) (((A) + ((B) / 2)) / (B))

uint16_t be16toh(uint16_t val);
