#include <nrf_soc.h>

#include "onboard-led.h"

void
onboard_led(enum onboard_led set)
{
        const int led_pin = 29;

        /* set to ouput */
        NRF_GPIO->PIN_CNF[led_pin] = GPIO_PIN_CNF_DIR_Output | GPIO_PIN_CNF_INPUT_Disconnect;
        switch (set) {
        case ONBOARD_LED_ON:
                NRF_GPIO->OUTSET = (1 << led_pin);
                break;
        case ONBOARD_LED_OFF:
                NRF_GPIO->OUTCLR = (1 << led_pin);
                break;
        case ONBOARD_LED_TOGGLE:
                NRF_GPIO->OUT ^= (1 << led_pin);
                break;
        }
}
