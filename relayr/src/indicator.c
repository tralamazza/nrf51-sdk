#include "simble.h"
#include "onboard-led.h"

struct indicator_ctx {
        struct service_desc;
        struct char_desc ind;
};


static struct indicator_ctx ind_ctx;


static void
ind_write_cb(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len)
{
        const uint8_t *datap = val;
        uint8_t data = *datap;

        switch (data) {
        case 0:
                onboard_led(ONBOARD_LED_OFF);
                break;
        case 1:
                onboard_led(ONBOARD_LED_ON);
                break;
        }
}

void
ind_init(void)
{
        struct indicator_ctx *ctx = &ind_ctx;

        simble_srv_init(ctx, simble_get_vendor_uuid_class(), VENDOR_UUID_IND_SERVICE);
        simble_srv_char_add(ctx, &ctx->ind,
                     simble_get_vendor_uuid_class(), VENDOR_UUID_IND_CHAR,
                     u8"Indicator LED",
                     1);
        ctx->ind.write_cb = ind_write_cb;
        simble_srv_register(ctx);
}
