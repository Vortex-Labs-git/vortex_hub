/**
 * @file device_table.c
 * @brief Devices paired with this hub (kept in RAM for the example)
 *
 * The hub does not know in advance what is around it: a device
 * becomes a row here when its CALLX passes the key check. Its type
 * (valve / plug / sensor unit) comes from the frame header, its id
 * and firmware version from the CALLX payload.
 *
 * RAM only: after a hub reboot the table is empty, devices send data,
 * the hub answers REJOIN, and they pair again (tests the REJOIN path).
 *
 * Every function takes s_lock, so the rx task, the link task and the
 * test button task can use the table at the same time.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "hub_fn/device_table.h"
#include "espnow_fn/espnow_util.h"


#define MAX_DEVICES     CONFIG_HUB_MAX_DEVICES

static const char *TAG = "DEVICES";

static hub_device_t      s_devices[MAX_DEVICES];
static SemaphoreHandle_t s_lock;



/*===============================================================
 *                  HELPERS (call with s_lock held)
 *==============================================================*/

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static hub_device_t *find(const uint8_t mac[6])
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_devices[i].used && memcmp(s_devices[i].mac, mac, 6) == 0) {
            return &s_devices[i];
        }
    }
    return NULL;
}

static hub_device_t *find_free(void)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_devices[i].used) {
            return &s_devices[i];
        }
    }
    return NULL;
}

static const char *type_name(uint8_t dev_type)
{
    const char *name = espnow_dev_topic_name(dev_type);
    return name ? name : "unknown";
}



/*===============================================================
 *                  PUBLIC
 *==============================================================*/

void device_table_init(void)
{
    memset(s_devices, 0, sizeof(s_devices));
    s_lock = xSemaphoreCreateMutex();
}

/**
 * @brief Add or refresh a device after a valid CALLX
 *
 * @return false if the table is full
 */
bool device_pair(const uint8_t mac[6], uint8_t dev_type, const espnow_callx_t *callx, int rssi)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    hub_device_t *dev = find(mac);
    bool is_new = (dev == NULL);

    if (is_new) {
        dev = find_free();
    }

    if (dev != NULL) {
        memset(dev, 0, sizeof(*dev));
        dev->used     = true;
        dev->dev_type = dev_type;
        memcpy(dev->mac, mac, 6);
        memcpy(dev->device_id,  callx->device_id,  ESPNOW_ID_LEN);
        memcpy(dev->fw_version, callx->fw_version, ESPNOW_FW_VER_LEN);
        dev->last_seen_ms = now_ms();
        dev->online       = true;
        dev->rssi         = rssi;

        ESP_LOGI(TAG, "%s %s %s (fw %s) " MACSTR,
                 is_new ? "NEW" : "RE-PAIRED", type_name(dev_type),
                 dev->device_id, dev->fw_version, MAC2STR(mac));
    }

    xSemaphoreGive(s_lock);
    return dev != NULL;
}

/**
 * @brief Mark a device as seen; copy its row to *out
 *
 * @return false if the MAC is not paired (caller answers REJOIN)
 */
bool device_seen(const uint8_t mac[6], int rssi, hub_device_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    hub_device_t *dev = find(mac);
    if (dev != NULL) {
        if (!dev->online) {
            ESP_LOGI(TAG, "%s %s back ONLINE", type_name(dev->dev_type), dev->device_id);
        }
        dev->last_seen_ms = now_ms();
        dev->online       = true;
        dev->rssi         = rssi;
        *out = *dev;
    }

    xSemaphoreGive(s_lock);
    return dev != NULL;
}

/**
 * @brief Copy every online device to out[] (send without holding the lock)
 *
 * @return number of devices copied
 */
int device_get_online(hub_device_t *out, int max)
{
    int count = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_DEVICES && count < max; i++) {
        if (s_devices[i].used && s_devices[i].online) {
            out[count++] = s_devices[i];
        }
    }
    xSemaphoreGive(s_lock);

    return count;
}

/**
 * @brief Mark devices offline when nothing came for timeout_s
 */
void device_check_timeouts(int timeout_s)
{
    int64_t now = now_ms();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_DEVICES; i++) {
        hub_device_t *dev = &s_devices[i];

        if (dev->used && dev->online && (now - dev->last_seen_ms) > (int64_t)timeout_s * 1000) {
            dev->online = false;
            ESP_LOGW(TAG, "%s %s OFFLINE (no data for %d s)", type_name(dev->dev_type), dev->device_id, timeout_s);
        }
    }
    xSemaphoreGive(s_lock);
}

/**
 * @brief Print the table (every minute from hub_link)
 */
void device_print_all(void)
{
    int64_t now = now_ms();
    int     count = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_DEVICES; i++) {
        hub_device_t *dev = &s_devices[i];
        if (!dev->used) continue;

        count++;
        ESP_LOGI(TAG, "  %-12s %-16s %-8s rssi %4d  last %llds ago  " MACSTR,
                 type_name(dev->dev_type), dev->device_id, dev->online ? "ONLINE" : "OFFLINE",
                 dev->rssi, (long long)((now - dev->last_seen_ms) / 1000), MAC2STR(dev->mac));
    }
    xSemaphoreGive(s_lock);

    if (count == 0) {
        ESP_LOGI(TAG, "  (no devices paired yet)");
    }
}
