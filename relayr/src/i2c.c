#include <nrf.h>
#include "i2c.h"

void
enable_i2c(void)
{
        NRF_TWI1->ENABLE = TWI_ENABLE_ENABLE_Enabled << TWI_ENABLE_ENABLE_Pos;
}

void
disable_i2c(void)
{
        NRF_TWI1->ENABLE = TWI_ENABLE_ENABLE_Disabled << TWI_ENABLE_ENABLE_Pos;
}
