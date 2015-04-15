#include <app_error.h>
#include <nrf_soc.h>
#include <nrf_sdm.h>
#include <pstorage.h>
#include <ble_gap.h>

#include "simble_central.h"
#include "app_trace.h"


static struct simble_central_ctx_t *global_ctx = NULL;


static struct simble_central_device_ctx_t*
find_client_by_conn(struct simble_central_ctx_t *ctx, uint16_t handle)
{
	for (int i = 0; i < DEVICE_MANAGER_MAX_CONNECTIONS; i++) {
		if (ctx->clients[i].db.conn_handle == handle) {
			return &ctx->clients[i];
		}
	}
	return NULL;
}

static ret_code_t
device_manager_event_handler(dm_handle_t const *p_handle,
	dm_event_t const *p_event, ret_code_t event_result)

{
	APP_ERROR_CHECK(event_result);
	uint32_t err_code;
	struct simble_central_device_ctx_t *client = &global_ctx->clients[p_handle->connection_id];
	switch(p_event->event_id) {
	case DM_EVT_CONNECTION:
		if (global_ctx->connect_cb) {
			global_ctx->connect_cb(p_handle, p_event);
		}
		global_ctx->client_count++;
		client->dm_handle = *p_handle;
		client->db.conn_handle = p_event->event_param.p_gap_param->conn_handle;
		app_trace_log("[simble_central]: client %p ble_db_discovery_start %x %x\r\n",
			client, client->dm_handle, client->db.conn_handle);
		err_code = ble_db_discovery_start(&client->db, client->db.conn_handle);
		APP_ERROR_CHECK(err_code);
		break;
	case DM_EVT_DISCONNECTION:
		client->db.conn_handle = 0;
		global_ctx->client_count--;
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
simble_central_ble_event_handler(struct simble_central_ctx_t *ctx, ble_evt_t *evt, 
	struct simble_central_device_ctx_t *client)
{
	uint32_t err_code;
	switch (evt->header.evt_id) {
	case BLE_GATTC_EVT_WRITE_RSP:
		if ((evt->evt.gattc_evt.gatt_status == BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION) ||
			(evt->evt.gattc_evt.gatt_status == BLE_GATT_STATUS_ATTERR_INSUF_ENCRYPTION)) {
			err_code = dm_security_setup_req(&client->dm_handle);
			APP_ERROR_CHECK(err_code);
		}
		break;
	case BLE_GATTC_EVT_HVX: /* notification/indication */
		break;
	case BLE_GATTC_EVT_TIMEOUT:
		break;
	default:
		break;
	}
}

void
simble_central_process_event_loop(struct simble_central_ctx_t *ctx)
{
	union {
		ble_evt_t evt;
		uint8_t buffer[(BLE_EVTS_PTR_ALIGNMENT * CEIL_DIV(sizeof(ble_evt_t) + GATT_MTU_SIZE_DEFAULT, BLE_EVTS_PTR_ALIGNMENT))];
	} ble_evt_buffer;
	struct simble_central_device_ctx_t *client = NULL;
	for (;;) {
		if (ctx->before_wait_cb) {
			ctx->before_wait_cb(ctx);
		}
		uint32_t evt_id;
		while (sd_evt_get(&evt_id) == NRF_SUCCESS) {
			pstorage_sys_event_handler(evt_id);
		}
		uint16_t ble_evt_buffer_len = sizeof(ble_evt_buffer);
		while (sd_ble_evt_get(ble_evt_buffer.buffer, &ble_evt_buffer_len) == NRF_SUCCESS) {
			dm_ble_evt_handler(&ble_evt_buffer.evt);
			client = find_client_by_conn(ctx, ble_evt_buffer.evt.evt.gattc_evt.conn_handle);
			if (client)
				ble_db_discovery_on_ble_evt(&client->db, &ble_evt_buffer.evt);
			simble_central_ble_event_handler(ctx, &ble_evt_buffer.evt, client);
			if (ctx->ble_event_handler_cb) {
				ctx->ble_event_handler_cb(ctx, &ble_evt_buffer.evt, client);
			}
			ble_evt_buffer_len = sizeof(ble_evt_buffer);
		}
		uint32_t err_code = sd_app_evt_wait();
		APP_ERROR_CHECK(err_code);
		sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
	}
}

static void
softdevice_assertion_handler(uint32_t pc, uint16_t line_number, const uint8_t *p_file_name)
{
	app_trace_log("[simble_central]: softdevice_assertion_handler 0x%x %d %s\r\n", pc, line_number, (char*)p_file_name);
	for (;;) {
		/* NOTHING */
	}
}

void
simble_central_init(const char *name, struct simble_central_ctx_t *ctx)
{
	app_trace_init();
	global_ctx = ctx;
	ctx->client_count = 0;
	// softdevice init
	uint32_t err_code = sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, softdevice_assertion_handler);
	APP_ERROR_CHECK(err_code);
	ble_enable_params_t ble_params = {
		.gap_enable_params = {
			.role = BLE_GAP_ROLE_CENTRAL,
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
	app_param.service_type = DM_PROTOCOL_CNTXT_GATT_CLI_ID;
	app_param.sec_param.bond = 1; // bonding
	app_param.sec_param.mitm = 0; // no mitm
	app_param.sec_param.io_caps = BLE_GAP_IO_CAPS_NONE;
	app_param.sec_param.oob = 0; // no oob
	app_param.sec_param.min_key_size = 7; // enabled: 7..16, disabled: 0
	app_param.sec_param.max_key_size = 16;
	err_code = dm_register(&ctx->app_id, &app_param);
	APP_ERROR_CHECK(err_code);
	// DB discovery
	memset(ctx->clients, 0, sizeof(ctx->clients));
	err_code = ble_db_discovery_init();
	APP_ERROR_CHECK(err_code);
}

uint32_t
simble_central_scan_start(struct simble_central_ctx_t *ctx)
{
	return sd_ble_gap_scan_start(&ctx->scan_params);
}

uint32_t
simble_central_enable_notification(uint16_t conn_handle, uint16_t cccd_handle,
	enum BLE_GATT_HVX_TYPES type)
{
	ble_gattc_write_params_t write_params;
	uint8_t buf[BLE_CCCD_VALUE_LEN] = { type, 0 };
	write_params.write_op = BLE_GATT_OP_WRITE_REQ;
	write_params.handle = cccd_handle;
	write_params.offset = 0;
	write_params.len = sizeof(buf);
	write_params.p_value = buf;
	return sd_ble_gattc_write(conn_handle, &write_params);
}

uint32_t
simble_central_connect(struct simble_central_ctx_t *ctx, ble_gap_addr_t *addr)
{
	return sd_ble_gap_connect(addr, &ctx->scan_params, &ctx->conn_params);
}
