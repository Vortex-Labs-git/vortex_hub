#ifndef DEVICE_TABLE_H
#define DEVICE_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "espnow_fn/espnow_proto.h"


/* One paired device (valve, plug, sensor unit — any mix) */
typedef struct {
    bool     used;
    uint8_t  mac[6];
    uint8_t  dev_type;                          // espnow_dev_type_t
    char     device_id[ESPNOW_ID_LEN + 1];
    char     fw_version[ESPNOW_FW_VER_LEN + 1];
    int64_t  last_seen_ms;
    bool     online;
    int      rssi;
} hub_device_t;


void device_table_init(void);

bool device_pair(const uint8_t mac[6], uint8_t dev_type, const espnow_callx_t *callx, int rssi);
bool device_seen(const uint8_t mac[6], int rssi, hub_device_t *out);

int  device_get_online(hub_device_t *out, int max);
void device_check_timeouts(int timeout_s);
void device_print_all(void);

#endif // DEVICE_TABLE_H
