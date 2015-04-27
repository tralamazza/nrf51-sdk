#include <stdlib.h>
#include <string.h>
#include <app_util.h>
#include <pstorage.h>

#include "simble.h"
#include "onboard-led.h"
#include "app_trace.h"

#define DFU_SERVICE_HANDLE 0x000C
#define BLE_HANDLE_MAX 0xFFFF

struct ble_gap_advdata {
        uint8_t length;
        uint8_t data[BLE_GAP_ADV_MAX_SIZE];
};

struct ble_gap_ad_header {
        uint8_t payload_length;
        uint8_t type;
} __packed;

struct ble_gap_ad_flags {
        struct ble_gap_ad_header;
        uint8_t flags;
} __packed;

struct ble_gap_ad_name {
        struct ble_gap_ad_header;
        uint8_t name[BLE_GAP_DEVNAME_MAX_LEN];
} __packed;


static struct service_desc *services;
static uint16_t current_conn_handle = BLE_CONN_HANDLE_INVALID;


static uint32_t
simble_add_advdata(const struct ble_gap_ad_header *data, struct ble_gap_advdata *advdata)
{
        size_t elem_length = data->payload_length + sizeof(*data);
        size_t final_length = advdata->length + elem_length;

        if (final_length > sizeof(advdata->data))
                return NRF_ERROR_DATA_SIZE;

        memcpy(&advdata->data[advdata->length], data, elem_length);
        advdata->data[advdata->length] = elem_length - 1;

        advdata->length += elem_length;

        return NRF_SUCCESS;
}

void
simble_adv_start(void)
{
        struct ble_gap_advdata advdata = { .length = 0 };

        struct ble_gap_ad_flags flags = {
                .payload_length = 1,
                .type = BLE_GAP_AD_TYPE_FLAGS,
                .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        };
        simble_add_advdata(&flags, &advdata);

        struct ble_gap_ad_name name;
        uint16_t namelen = sizeof(advdata);
        if (sd_ble_gap_device_name_get(name.name, &namelen) != NRF_SUCCESS)
                namelen = 0;
        name.type =  BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
        name.payload_length = namelen;
        simble_add_advdata(&name, &advdata);

        sd_ble_gap_adv_data_set(advdata.data, advdata.length, NULL, 0);

        ble_gap_adv_params_t adv_params = {
                .type = BLE_GAP_ADV_TYPE_ADV_IND,
                .fp = BLE_GAP_ADV_FP_ANY,
                .interval = 0x400,
        };
        sd_ble_gap_adv_start(&adv_params);
        onboard_led(ONBOARD_LED_ON);
}

static void
simble_srv_tx_init(void)
{
        uint16_t srv_handle;
        ble_uuid_t srv_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x1804};
        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &srv_uuid,
                                 &srv_handle);

        ble_gatts_char_handles_t chr_handles;
        ble_gatts_char_md_t char_meta = {.char_props = {.read = 1}};
        ble_uuid_t chr_uuid = {.type = BLE_UUID_TYPE_BLE, .uuid = 0x2a07};
        ble_gatts_attr_md_t chr_attr_meta = {
                .vloc = BLE_GATTS_VLOC_STACK,
        };
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);
        uint8_t tx_val = 0;
        ble_gatts_attr_t chr_attr = {
                .p_uuid = &chr_uuid,
                .p_attr_md = &chr_attr_meta,
                .init_offs = 0,
                .init_len = sizeof(tx_val),
                .max_len = sizeof(tx_val),
                .p_value = &tx_val,
        };
        sd_ble_gatts_characteristic_add(srv_handle,
                                        &char_meta,
                                        &chr_attr,
                                        &chr_handles);
}

/* DM requires a `app_error_handler` */
void __attribute__((weak))
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
        app_trace_log("[simble]: err: %d, line: %d, file: %s\r\n", error_code, line_num, p_file_name);
        for (;;) {
                /* NOTHING */
        }
}

void
simble_init(const char *name)
{
        app_trace_init();
        sd_softdevice_enable(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL); /* XXX assertion handler */

        ble_enable_params_t ble_params = {
#if defined(SD120)
                .gap_enable_params = {
                        .role = BLE_GAP_ROLE_PERIPH,
                },
#endif
                .gatts_enable_params = {
                        .service_changed = 1,
                },
        };
        sd_ble_enable(&ble_params);

        ble_gap_conn_sec_mode_t mode;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&mode);
        sd_ble_gap_device_name_set(&mode, (const uint8_t *)name, strlen(name));

        // flash storage
        pstorage_init();

        simble_srv_tx_init();
}

uint8_t
simble_get_vendor_uuid_class(void)
{
        ble_uuid128_t vendorid = {{0x13,0xb0,0xa0,0x71,0xfe,0x62,0x4c,0x01,0xaa,0x4d,0xd8,0x03,0,0,0x0b,0xd0}};
        static uint8_t vendor_type = BLE_UUID_TYPE_UNKNOWN;

        if (vendor_type != BLE_UUID_TYPE_UNKNOWN)
                return (vendor_type);

        sd_ble_uuid_vs_add(&vendorid, &vendor_type);
        return (vendor_type);
}

void
simble_srv_register(struct service_desc *s)
{
        s->next = services;
        services = s;

        sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                 &s->uuid,
                                 &s->handle);

        for (int i = 0; i < s->char_count; ++i) {
                struct char_desc *c = &s->chars[i];
                ble_gatts_attr_md_t cccd_md;
                memset(&cccd_md, 0, sizeof(cccd_md));
                BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
                BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
                cccd_md.vloc = BLE_GATTS_VLOC_STACK;
                int have_write = c->write_cb != NULL;
                ble_gatts_char_md_t char_meta = {
                        .char_props = {
                                .broadcast = 0,
                                .read = 1,
                                .write_wo_resp = have_write,
                                .write = have_write,
                                .notify = c->notify,
                                .indicate = c->indicate,
                                .auth_signed_wr = have_write,
                        },
                        .p_char_user_desc = (uint8_t *)c->desc,
                        .char_user_desc_size = strlen(c->desc),
                        .char_user_desc_max_size = strlen(c->desc),
                        .p_char_pf = c->format.format != 0 ? &c->format : NULL,
                        .p_cccd_md = (c->notify || c->indicate) ? &cccd_md : NULL,
                };
                ble_gatts_attr_md_t chr_attr_meta = {
                        .vloc = BLE_GATTS_VLOC_STACK,
                        .rd_auth = 1,
                        .wr_auth = 1,
                };
                BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.read_perm);
                if (have_write)
                        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&chr_attr_meta.write_perm);
                else
                        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&chr_attr_meta.write_perm);

                ble_gatts_attr_t chr_attr = {
                        .p_uuid = &c->uuid,
                        .p_attr_md = &chr_attr_meta,
                        .init_offs = 0,
                        .init_len = 0,
                        .max_len = c->length,
                };
                sd_ble_gatts_characteristic_add(s->handle,
                                                &char_meta,
                                                &chr_attr,
                                                &c->handles);
        }
}

void
simble_srv_init(struct service_desc *s, uint8_t type, uint16_t id)
{
        *s = (struct service_desc){
                .uuid = {.type = type,
                         .uuid = id},
                .char_count = 0,
        };
};

void
simble_srv_char_add(struct service_desc *s, struct char_desc *c, uint8_t type, uint16_t id, const char *desc, uint16_t length)
{
        *c = (struct char_desc){
                .uuid = {
                        .type = type,
                        .uuid = id
                },
                .desc = desc,
                .length = length,
        };
        s->char_count++;
}

void
simble_srv_char_attach_format(struct char_desc *c, uint8_t format, int8_t exponent, uint16_t unit)
{
        c->format = (ble_gatts_char_pf_t){
                .format = format,
                .exponent = exponent,
                .unit = unit,
        };
}

void
simble_srv_char_update(struct char_desc *c, void *val)
{
        ble_gatts_value_t vt = {
                .len = c->length,
                .offset = 0,
                .p_value = val
        };
        sd_ble_gatts_value_set(current_conn_handle, c->handles.value_handle, &vt);
}

uint32_t
simble_srv_char_notify(struct char_desc *c, bool indicate, uint16_t length, void *val)
{
        ble_gatts_hvx_params_t hvx_params = {
                .handle = c->handles.value_handle,
                .type = indicate ? BLE_GATT_HVX_INDICATION : BLE_GATT_HVX_NOTIFICATION,
                .offset = 0,
                .p_len = &length,
                .p_data = val,
        };
        return sd_ble_gatts_hvx(current_conn_handle, &hvx_params);
}

static struct service_desc *
srv_find_by_uuid(ble_uuid_t *uuid)
{
        struct service_desc *s = services;

        for (; s != NULL; s = s->next) {
                if (memcmp(&s->uuid, uuid, sizeof(ble_uuid_t)) == 0)
                        break;
        }

        return (s);
}

static struct char_desc *
srv_find_char_by_uuid(struct service_desc *s, ble_uuid_t *uuid)
{
        struct char_desc *c = s->chars;

        for (int i = 0; i < s->char_count; ++i, ++c) {
                if (memcmp(&c->uuid, uuid, sizeof(ble_uuid_t)) == 0)
                        return (c);
        }
        return (NULL);
}

typedef void (srv_foreach_cb_t)(struct service_desc *s);

static void
srv_foreach_srv(srv_foreach_cb_t *cb)
{
        struct service_desc *s = services;

        for (; s != NULL; s = s->next)
                cb(s);
}

static void
srv_notify_connect(struct service_desc *s)
{
        if (s->connect_cb)
                s->connect_cb(s);
}

static void
srv_notify_disconnect(struct service_desc *s)
{
        if (s->disconnect_cb)
                s->disconnect_cb(s);
}

static void
srv_handle_ble_event(ble_evt_t *evt)
{
        struct service_desc *s;
        struct char_desc *c;
        app_trace_log("[simble]: BLE event %d\r\n", evt->header.evt_id);
        switch (evt->header.evt_id) {
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST: {
                ble_gatts_evt_rw_authorize_request_t *auth_req = &evt->evt.gatts_evt.params.authorize_request;
                if (auth_req->type == BLE_GATTS_AUTHORIZE_TYPE_INVALID)
                        break;
                ble_gatts_rw_authorize_reply_params_t auth_reply = {
                        .type = auth_req->type,
                };
                if (auth_reply.type == BLE_GATTS_AUTHORIZE_TYPE_READ) {
                        ble_gatts_attr_context_t *ctx = &auth_req->request.read.context;
                        s = srv_find_by_uuid(&ctx->srvc_uuid);
                        if (s == NULL) break;
                        c = srv_find_char_by_uuid(s, &ctx->char_uuid);
                        if (c == NULL) break;
                        auth_reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
                        if (c->read_cb) {
                                auth_reply.params.read.update = 1;
                                c->read_cb(s, c, (void*)&auth_reply.params.read.p_data, &auth_reply.params.read.len);
                        }
                } else { // BLE_GATTS_AUTHORIZE_TYPE_WRITE
                        if ((auth_req->request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ) ||
                            (auth_req->request.write.op == BLE_GATTS_OP_WRITE_REQ)) {
                                s = srv_find_by_uuid(&auth_req->request.write.context.srvc_uuid);
                                if (s == NULL) break;
                                c = srv_find_char_by_uuid(s, &auth_req->request.write.context.char_uuid);
                                if (c == NULL) break;
                                if (c->write_cb)
                                        c->write_cb(s, c, auth_req->request.write.data, auth_req->request.write.len, auth_req->request.write.offset);
                        }
                        auth_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                }
                sd_ble_gatts_rw_authorize_reply(evt->evt.gatts_evt.conn_handle, &auth_reply);
                break;
        }
        case BLE_EVT_USER_MEM_REQUEST:
                sd_ble_user_mem_reply(current_conn_handle, NULL);
                break;
        case BLE_GAP_EVT_CONNECTED:
                current_conn_handle = evt->evt.gap_evt.conn_handle;
                srv_foreach_srv(srv_notify_connect);
                sd_ble_gatts_service_changed(current_conn_handle, DFU_SERVICE_HANDLE, BLE_HANDLE_MAX);
                onboard_led(ONBOARD_LED_OFF);
                break;
        case BLE_GAP_EVT_DISCONNECTED:
                current_conn_handle = BLE_CONN_HANDLE_INVALID;
                srv_foreach_srv(srv_notify_disconnect);
                simble_adv_start();
                break;
        case BLE_GATTS_EVT_WRITE: {
                ble_gatts_evt_write_t *write_evt = &evt->evt.gatts_evt.params.write;
                s = srv_find_by_uuid(&write_evt->context.srvc_uuid);
                if (s == NULL) break;
                c = srv_find_char_by_uuid(s, &write_evt->context.char_uuid);
                if (c == NULL) break;
                if (write_evt->handle == c->handles.cccd_handle) {
                        if (c->notify_status_cb)
                                c->notify_status_cb(s, c, uint16_decode(write_evt->data));
                } else {
                        c->write_cb(s, c, write_evt->data, write_evt->len, write_evt->offset);
                }
                break;
        }
        case BLE_GATTS_EVT_HVC:
                s = services;
                for (; s != NULL; s = s->next) {
                        c = s->chars;
                        for (int i = 0; i < s->char_count; ++i, ++c) {
                                if (c->handles.value_handle == evt->evt.gatts_evt.params.hvc.handle) {
                                        if (c->indicated_cb)
                                                c->indicated_cb(s, c);
                                        break;
                                }
                        }
                }
                break;
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
                sd_ble_gatts_sys_attr_set(current_conn_handle, NULL, 0,
                        BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS);
                break;
        }
}

void
simble_process_event_loop(void)
{
        union {
                ble_evt_t evt;
                uint8_t buffer[(BLE_EVTS_PTR_ALIGNMENT * CEIL_DIV(sizeof(ble_evt_t) + GATT_MTU_SIZE_DEFAULT, BLE_EVTS_PTR_ALIGNMENT))];
        } ble_evt_buffer;
        for (;;) {
                uint32_t evt_id;
                while (sd_evt_get(&evt_id) == NRF_SUCCESS) {
                        pstorage_sys_event_handler(evt_id);
                }
                uint16_t ble_evt_buffer_len = sizeof(ble_evt_buffer);
                while (sd_ble_evt_get(ble_evt_buffer.buffer, &ble_evt_buffer_len) == NRF_SUCCESS) {
                        srv_handle_ble_event(&ble_evt_buffer.evt);
                        ble_evt_buffer_len = sizeof(ble_evt_buffer);
                }
                sd_app_evt_wait();
                sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
        }
}
