#ifndef SIMBLE_H
#define SIMBLE_H

#include <ble.h>
#include <nrf_sdm.h>

enum vendor_uuid {
        VENDOR_UUID_SENSOR_SERVICE = 0x1801,
        VENDOR_UUID_IND_SERVICE = 0x1802,
        VENDOR_UUID_TEMP_CHAR = 0x2301,
        VENDOR_UUID_HUMID_CHAR = 0x2302,
        VENDOR_UUID_MOTION_CHAR = 0x2303,
        VENDOR_UUID_SOUND_CHAR = 0x2305,
        VENDOR_UUID_BRIGHT_CHAR = 0x2306,
        VENDOR_UUID_COLOR_CHAR = 0x2307,
        VENDOR_UUID_PROXIMITY_CHAR = 0x2308,
        VENDOR_UUID_IND_CHAR = 0x2309,
        VENDOR_UUID_ADC_CHAR = 0x230a,
        VENDOR_UUID_IR_CHAR = 0x2310,
};

enum org_bluetooth_unit {
        ORG_BLUETOOTH_UNIT_UNITLESS = 0x2700,
        ORG_BLUETOOTH_UNIT_DEGREE_CELSIUS = 0x272f,
        ORG_BLUETOOTH_UNIT_PERCENTAGE = 0x27AD,
};

struct char_desc;
struct service_desc;

typedef void (char_write_cb_t)(struct service_desc *s, struct char_desc *c, const void *val, const uint16_t len);
typedef void (char_read_cb_t)(struct service_desc *s, struct char_desc *c, void **val, uint16_t *len);
typedef void (connect_cb_t)(struct service_desc *s);
typedef void (disconnect_cb_t)(struct service_desc *s);

struct char_desc {
        ble_uuid_t uuid;
        const char *desc;
        /* fmt */
        uint16_t length;
        uint16_t handle;
        ble_gatts_char_pf_t format;
        char_write_cb_t *write_cb;
        char_read_cb_t *read_cb;
        void *data;
};

struct service_desc {
        struct service_desc *next;
        ble_uuid_t uuid;
        uint16_t handle;
        connect_cb_t *connect_cb;
        disconnect_cb_t *disconnect_cb;
        uint8_t char_count;     /* XXX ugly */
        struct char_desc chars[];
};


void simble_init(const char *name);
void simble_adv_start(void);
uint8_t simble_get_vendor_uuid_class(void);
void simble_process_event_loop(void);

void simble_srv_register(struct service_desc *s);
void simble_srv_init(struct service_desc *s, uint8_t type, uint16_t id);
void simble_srv_char_add(struct service_desc *s, struct char_desc *c, uint8_t type, uint16_t id, const char *desc, uint16_t length);
void simble_srv_char_attach_format(struct char_desc *c, uint8_t format, int8_t exponent, uint16_t unit);
void simble_srv_char_update(struct char_desc *c, void *val);

#endif
