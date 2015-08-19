#pragma once

#include <ble.h>
#include <device_manager.h>

enum BLE_GATT_HVX_TYPES {
	BLE_GATT_HVX_TYPES_INVALID = BLE_GATT_HVX_INVALID,
	BLE_GATT_HVX_TYPES_NOTIFICATION = BLE_GATT_HVX_NOTIFICATION,
	BLE_GATT_HVX_TYPES_INDICATION = BLE_GATT_HVX_INDICATION,
};

struct simble_central_ctx_t;

struct simble_central_device_ctx_t {
	uint16_t conn_handle;
	dm_handle_t dm_handle;
};

typedef void (device_connect_cb_t) (dm_handle_t const *p_handle,
	dm_event_t const *p_event);
typedef void (device_disconnect_cb_t) (dm_handle_t const *p_handle,
	dm_event_t const *p_event);
typedef void (ble_event_handler_cb_t) (struct simble_central_ctx_t *ctx, ble_evt_t *evt,
	uint16_t evt_len, struct simble_central_device_ctx_t *client);
typedef void (before_wait_cb_t) (struct simble_central_ctx_t *ctx);

struct simble_central_ctx_t {
	char *name;
	ble_gap_scan_params_t scan_params;
	ble_gap_conn_params_t conn_params;
	device_connect_cb_t *connect_cb;
	device_disconnect_cb_t *disconnect_cb;
	ble_event_handler_cb_t *ble_event_handler_cb;
	before_wait_cb_t *before_wait_cb;
	dm_application_instance_t app_id;
	uint8_t client_count;
	struct simble_central_device_ctx_t clients[DEVICE_MANAGER_MAX_CONNECTIONS];
};

void simble_central_init(const char *name, struct simble_central_ctx_t *ctx);
void simble_central_process_event_loop(struct simble_central_ctx_t *ctx) __attribute__ ((noreturn));
uint32_t simble_central_scan_start(struct simble_central_ctx_t *ctx);
uint32_t simble_central_enable_notification(uint16_t conn_handle, uint16_t cccd_handle,
	enum BLE_GATT_HVX_TYPES type);
uint32_t simble_central_connect(struct simble_central_ctx_t *ctx, ble_gap_addr_t *addr);
