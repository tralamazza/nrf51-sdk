#include "simble.h"
#include <ble_srv_common.h>

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS   1200                                        /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION    3                                           /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) \
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / 255) * ADC_PRE_SCALING_COMPENSATION) /** <Convert the result of ADC conversion in millivolts. */


struct batt_serv_ctx {
	struct service_desc;
	struct char_desc batt_lvl;
	uint8_t last_reading;
};

static struct batt_serv_ctx batt_serv_ctx;

// void
// ADC_IRQHandler(void)
// {
// 	if (NRF_ADC->EVENTS_END == 0) {
// 		return;
// 	}
//
// 	uint16_t    batt_lvl_in_milli_volts;
//
// 	NRF_ADC->EVENTS_END     = 0;
// 	batt_serv_ctx.last_reading = NRF_ADC->RESULT;
// 	NRF_ADC->TASKS_STOP     = 1;
//
// 	batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(batt_serv_ctx.last_reading);
// 	batt_serv_ctx.last_reading = battery_level_in_percent(batt_lvl_in_milli_volts);
//
// 	simble_srv_char_update(&batt_serv_ctx.batt_lvl, &batt_serv_ctx.last_reading);
// }

static void
adc_config()
{
	NRF_ADC->CONFIG = (ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos)     |
			(ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos)  |
			(ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)  |
			(ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos)    |
			(ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
}

static void
adc_read_start()
{
	NRF_ADC->EVENTS_END = 0;
	NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;
	sd_nvic_ClearPendingIRQ(ADC_IRQn);
	sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);
	sd_nvic_EnableIRQ(ADC_IRQn);
	adc_config();
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	NRF_ADC->TASKS_START = 1;
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
	adc_config();
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	NRF_ADC->TASKS_START = 1;
	while (NRF_ADC->EVENTS_END == 0) {
		// __asm("nop");
	}

	NRF_ADC->EVENTS_END     = 0;
	uint16_t    batt_lvl_in_milli_volts;
	uint16_t result = NRF_ADC->RESULT;
	batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(result);
	result = battery_level_in_percent(batt_lvl_in_milli_volts);
	NRF_ADC->TASKS_STOP     = 1;
	return result;
}

static void
batt_serv_connect_cb(struct service_desc *s)
{
  adc_read_start();
}

static void
batt_serv_disconnect_cb(struct service_desc *s)
{
	//
}

static void
batt_lvl_read_cb(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len)
{
  struct batt_serv_ctx *ctx = (struct batt_serv_ctx *) s;
	ctx->last_reading = adc_read_blocking();
	*val = &ctx->last_reading;
	*len = 1;
}

void
batt_serv_init(void)
{
	struct batt_serv_ctx *ctx = &batt_serv_ctx;

	ctx->last_reading = 0;
	// init the service context
	simble_srv_init(ctx, BLE_UUID_TYPE_BLE, BLE_UUID_BATTERY_SERVICE);
	// add a characteristic to our service
	simble_srv_char_add(ctx, &ctx->batt_lvl,
		BLE_UUID_TYPE_BLE, BLE_UUID_BATTERY_LEVEL_CHAR,
		u8"Battery Level",
		1); // size in bytes
  simble_srv_char_attach_format(&ctx->batt_lvl, BLE_GATT_CPF_FORMAT_UINT8,
    0, ORG_BLUETOOTH_UNIT_PERCENTAGE);
	// BLE callbacks (optional)
  //	ctx->connect_cb = batt_serv_connect_cb;
  //	ctx->disconnect_cb = batt_serv_disconnect_cb;
	ctx->batt_lvl.read_cb = batt_lvl_read_cb;
	simble_srv_register(ctx); // register our service
}
