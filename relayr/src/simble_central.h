#pragma once

#include <ble.h>
#include <device_manager.h>

struct simble_central_ctx_t;

typedef void (device_connect_cb_t) (dm_handle_t const *p_handle,
	dm_event_t const *p_event);
typedef void (device_disconnect_cb_t) (dm_handle_t const *p_handle,
	dm_event_t const *p_event);
typedef void (ble_event_handler_cb) (struct simble_central_ctx_t *ctx, ble_evt_t *evt);
typedef void (before_wait_cb_t) (struct simble_central_ctx_t *ctx);

struct simble_central_ctx_t {
	char *name;
	bool central;
	ble_gap_scan_params_t scan_params;
	ble_gap_conn_params_t conn_params;
	device_connect_cb_t *connect_cb;
	device_disconnect_cb_t *disconnect_cb;
	ble_event_handler_cb *ble_event_handler_cb;
	before_wait_cb_t *before_wait_cb;
	dm_application_instance_t app_id;
};

void simble_central_init(const char *name, struct simble_central_ctx_t *ctx);
void simble_central_process_event_loop(struct simble_central_ctx_t *ctx) __attribute__ ((noreturn));
bool simble_central_scan_start(struct simble_central_ctx_t *ctx);
