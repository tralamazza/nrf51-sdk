#include "simble.h"
#include <ble_srv_common.h>
#include "rtc.h"

#define DEFAULT_SAMPLING_PERIOD 1000UL
#define MIN_SAMPLING_PERIOD 200UL

#define BATT_NOTIF_TIMER_ID  3

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS   1200                                        /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION    3                                           /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) \
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / 255) * ADC_PRE_SCALING_COMPENSATION) /** <Convert the result of ADC conversion in millivolts. */


struct batt_serv_ctx {
	struct service_desc;
	struct char_desc batt_lvl;
        struct char_desc sampling_period_batt_lvl;
	uint8_t last_reading;
        uint32_t sampling_period;
};

static struct batt_serv_ctx batt_serv_ctx;


static void
adc_config()
{
	NRF_ADC->CONFIG = (ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos)     |
			(ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos)  |
			(ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)  |
			(ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos)    |
			(ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
}

static uint16_t
adc_read_blocking()
{
	while (NRF_ADC->BUSY == 1) {
		// __asm("nop");
	}
	NRF_ADC->EVENTS_END = 0;
	sd_nvic_DisableIRQ(ADC_IRQn);
	NRF_ADC->INTENCLR = ADC_INTENCLR_END_Enabled;
	NRF_ADC->TASKS_START = 1;
        adc_config();
        NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	while (NRF_ADC->EVENTS_END == 0) {
		// __asm("nop");
	}
	uint16_t    batt_lvl_in_milli_volts;
	uint16_t result = NRF_ADC->RESULT;
	batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(result);
	result = battery_level_in_percent(batt_lvl_in_milli_volts);
        NRF_ADC->EVENTS_END     = 0;
	NRF_ADC->TASKS_STOP     = 1;
	return result;
}

static void
batt_serv_disconnected()
{
        rtc_update_cfg(batt_serv_ctx.sampling_period, (uint8_t)BATT_NOTIF_TIMER_ID, false);
}

static void
batt_lvl_read_cb(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len)
{
        struct batt_serv_ctx *ctx = (struct batt_serv_ctx *) s;
	ctx->last_reading = adc_read_blocking();
	*val = &ctx->last_reading;
	*len = 1;
}

static void
sampling_period_read_cb(struct service_desc *s, struct char_desc *c, void **valp, uint16_t *lenp)
{
	struct batt_serv_ctx *ctx = (struct batt_serv_ctx *)s;
	*valp = &ctx->sampling_period;
	*lenp = sizeof(ctx->sampling_period);
}

static void
sampling_period_write_cb(struct service_desc *s, struct char_desc *c,
        const void *val, const uint16_t len)
{
	struct batt_serv_ctx *ctx = (struct batt_serv_ctx *)s;
        if (*(uint32_t*)val > MIN_SAMPLING_PERIOD)
                ctx->sampling_period = *(uint32_t*)val;
        else
                ctx->sampling_period = MIN_SAMPLING_PERIOD;
        rtc_update_cfg(ctx->sampling_period, (uint8_t)BATT_NOTIF_TIMER_ID, true);
}

void
notify_status_cb(struct service_desc *s, struct char_desc *c, const int8_t status)
{
        struct batt_serv_ctx *ctx = (struct batt_serv_ctx *)s;

        if (status & BLE_GATT_HVX_NOTIFICATION)
                rtc_update_cfg(ctx->sampling_period, (uint8_t)BATT_NOTIF_TIMER_ID, true);
        else     //disable NOTIFICATION_TIMER
                rtc_update_cfg(ctx->sampling_period, (uint8_t)BATT_NOTIF_TIMER_ID, false);
}

static void
batt_lvl_notif_timer_cb(struct rtc_ctx *ctx)
{
        void *val = &batt_serv_ctx.last_reading;
	uint16_t len = sizeof(batt_serv_ctx.last_reading);
	batt_lvl_read_cb(&batt_serv_ctx, &batt_serv_ctx.batt_lvl, &val, &len);
	simble_srv_char_notify(&batt_serv_ctx.batt_lvl, false, len, val);
}

void
batt_serv_init(struct rtc_ctx *rtc_ctx)
{
        struct batt_serv_ctx *ctx = &batt_serv_ctx;

        ctx->last_reading = 0;
        // init the service context
        simble_srv_init(ctx, BLE_UUID_TYPE_BLE, BLE_UUID_BATTERY_SERVICE);
        // add a characteristic to our service
        simble_srv_char_add(ctx, &ctx->batt_lvl, BLE_UUID_TYPE_BLE,
                BLE_UUID_BATTERY_LEVEL_CHAR, u8"Battery Level", 1); // size in bytes
        simble_srv_char_attach_format(&ctx->batt_lvl, BLE_GATT_CPF_FORMAT_UINT8,
                0, ORG_BLUETOOTH_UNIT_PERCENTAGE);

        simble_srv_char_add(ctx, &ctx->sampling_period_batt_lvl,
        	simble_get_vendor_uuid_class(), VENDOR_UUID_SAMPLING_PERIOD_CHAR,
        	u8"sampling period", sizeof(ctx->sampling_period)); // size in bytes
        // Resolution: 1ms, max value: 16777216 (4 hours)
        // A value of 0 will disable periodic notifications
        simble_srv_char_attach_format(&ctx->sampling_period_batt_lvl,
                BLE_GATT_CPF_FORMAT_UINT24, 0, ORG_BLUETOOTH_UNIT_UNITLESS);

        ctx->batt_lvl.read_cb = batt_lvl_read_cb;
        ctx->disconnect_cb = batt_serv_disconnected;
        ctx->batt_lvl.notify = 1;
        ctx->batt_lvl.notify_status_cb = notify_status_cb;
        ctx->sampling_period_batt_lvl.read_cb = sampling_period_read_cb;
        ctx->sampling_period_batt_lvl.write_cb = sampling_period_write_cb;

        batt_serv_ctx.sampling_period = DEFAULT_SAMPLING_PERIOD;
        rtc_ctx->rtc_x[BATT_NOTIF_TIMER_ID].type = PERIODIC;
        rtc_ctx->rtc_x[BATT_NOTIF_TIMER_ID].period = batt_serv_ctx.sampling_period;
        rtc_ctx->rtc_x[BATT_NOTIF_TIMER_ID].enabled = false;
        rtc_ctx->rtc_x[BATT_NOTIF_TIMER_ID].cb = batt_lvl_notif_timer_cb;

        simble_srv_register(ctx); // register our service
}
