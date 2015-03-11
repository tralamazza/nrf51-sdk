#include "util.h"

uint16_t
be16toh(uint16_t val)
{
        return ((val & 0xff) << 8 | (val >> 8));
}
