#include "simble_central.h"
#include <app_error.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <pstorage.h>
#include <ble_gap.h>

const uint32_t BLE_EVT_BUF_SIZE = (sizeof(ble_evt_t) + (GATT_MTU_SIZE_DEFAULT));

struct simble_central_ctx_t *global_ctx;

static api_result_t
device_manager_event_handler(dm_handle_t const *p_handle,
	dm_event_t const *p_event, api_result_t event_result)

{
	APP_ERROR_CHECK(event_result);
	uint32_t err_code;
	switch(p_event->event_id) {
	case DM_EVT_CONNECTION:
		if (global_ctx->connect_cb) {
			global_ctx->connect_cb(p_handle, p_event);
		}
		break;
	case DM_EVT_DISCONNECTION:
		if (global_ctx->disconnect_cb) {
			global_ctx->disconnect_cb(p_handle, p_event);
		}
		break;
	case DM_EVT_LINK_SECURED:
		break;
	case DM_EVT_SECURITY_SETUP: /* won't fire for previously bonded devices */
		err_code = dm_security_setup_req((dm_handle_t*)p_handle);
		APP_ERROR_CHECK(err_code);
	case DM_EVT_SECURITY_SETUP_COMPLETE: /* won't fire for previously bonded devices */
		break;
	case DM_EVT_SECURITY_SETUP_REFRESH:
		break;
	case DM_EVT_DEVICE_CONTEXT_LOADED:
	case DM_EVT_DEVICE_CONTEXT_STORED:
	case DM_EVT_DEVICE_CONTEXT_DELETED:
	case DM_EVT_SERVICE_CONTEXT_LOADED:
	case DM_EVT_SERVICE_CONTEXT_STORED:
	case DM_EVT_SERVICE_CONTEXT_DELETED:
	case DM_EVT_APPL_CONTEXT_LOADED:
	case DM_EVT_APPL_CONTEXT_STORED:
	case DM_EVT_APPL_CONTEXT_DELETED:
		break;
	}
	return NRF_SUCCESS;
}

static void
handle_soc_event(uint32_t evt_id)
{
	switch(evt_id) {
	case NRF_EVT_HFCLKSTARTED:
	case NRF_EVT_POWER_FAILURE_WARNING:
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
	case NRF_EVT_FLASH_OPERATION_ERROR:
		break;
	}
}

void
simble_central_process_event_loop(struct simble_central_ctx_t *ctx)
{
	uint32_t evt_id;
	uint32_t ble_evt_buffer[CEIL_DIV(BLE_EVT_BUF_SIZE, sizeof(uint32_t))];
	uint16_t ble_evt_buffer_len = sizeof(ble_evt_buffer);
	for (;;) {
		if (ctx->before_wait_cb) {
			ctx->before_wait_cb(ctx);
		}
		while (sd_evt_get(&evt_id) == NRF_SUCCESS) {
			handle_soc_event(evt_id);
		}
		while (sd_ble_evt_get((uint8_t*)&ble_evt_buffer, &ble_evt_buffer_len) == NRF_SUCCESS) {
			dm_ble_evt_handler((ble_evt_t*)&ble_evt_buffer);
			if (ctx->ble_event_handler_cb) {
				ctx->ble_event_handler_cb(ctx, (ble_evt_t*)&ble_evt_buffer);
			}
			ble_evt_buffer_len = sizeof(ble_evt_buffer);
		}
		sd_app_evt_wait();
	}
}

static void
softdevice_assertion_handler(uint32_t pc, uint16_t line_number, const uint8_t *p_file_name)
{
	// XXX save to flash?
}

void
simble_central_init(const char *name, struct simble_central_ctx_t *ctx)
{
	global_ctx = ctx;
	// softdevice init
	uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, softdevice_assertion_handler);
	APP_ERROR_CHECK(err_code);
	ble_enable_params_t ble_params = {
		.gap_enable_params = {
			.role = ctx->central ? BLE_GAP_ROLE_CENTRAL : BLE_GAP_ROLE_PERIPH,
		},
	};
	// s120 2.0 initialization
	err_code = sd_ble_enable(&ble_params);
	APP_ERROR_CHECK(err_code);
	// device name
	ble_gap_conn_sec_mode_t sec_mode;
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&sec_mode);
	err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)name, strlen(name));
	APP_ERROR_CHECK(err_code);
	// flash storage
	err_code = pstorage_init();
	APP_ERROR_CHECK(err_code);
	// device manager
	dm_init_param_t init_param;
	init_param.clear_persistent_data = false;
	err_code = dm_init(&init_param);
	APP_ERROR_CHECK(err_code);
	dm_application_param_t app_param;
	memset(&app_param.sec_param, 0, sizeof(ble_gap_sec_params_t));
	app_param.evt_handler = device_manager_event_handler;
	app_param.service_type = ctx->central ? DM_PROTOCOL_CNTXT_GATT_CLI_ID : DM_PROTOCOL_CNTXT_GATT_SRVR_ID;
	app_param.sec_param.bond = ctx->central ? 1 : 0; // bonding
	app_param.sec_param.mitm = 0; // no mitm
	app_param.sec_param.io_caps = BLE_GAP_IO_CAPS_NONE;
	app_param.sec_param.oob = 0; // no oob
	app_param.sec_param.min_key_size = 0; // enabled: 7..16, disabled: 0
	app_param.sec_param.max_key_size = 16;
	err_code = dm_register(&ctx->app_id, &app_param);
	APP_ERROR_CHECK(err_code);
}

bool
simble_central_scan_start(struct simble_central_ctx_t *ctx)
{
	uint32_t err_code = sd_ble_gap_scan_start(&ctx->scan_params);
	// NRF_ERROR_BUSY The stack is busy, process pending events and retry.
	if (err_code == NRF_ERROR_BUSY) {
		return false;
	}
	APP_ERROR_CHECK(err_code);
	return true;
}
