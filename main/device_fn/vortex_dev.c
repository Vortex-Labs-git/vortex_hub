/**
 * @file vortex_dev.c
 * @brief Device objects (local DB): created at boot from the device list
 *
 * At boot, for every device in hubDeviceList (NVS):
 *
 *   valve  VA202611301  ->  vortexValves[0]   topic_base "vortex_device/wifi_valve/VA202611301"
 *   sensor SU202611001  ->  vortexSensors[0]  topic_base "vortex_device/sensor_unit/SU202611001"
 *   plug   SP202611001  ->  vortexPlugs[0]    topic_base "vortex_device/smart_plug/SP202611001"
 *
 * Every object starts with a Vortex_Dev (common part), so one list of
 * Vortex_Dev pointers covers all types; the type says which object it is:
 *   (Vortex_VA *)dev   when dev->type == DEV_VALVE   ... and so on.
 *
 * A message is matched to its object by its topic:
 *   vortex_device/<type>/<device_id>/<sub>    sub = status, state_data, cmd_data ...
 */

#include <string.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "device_fn/vortex_dev.h"
#include "device_fn/vortex_va.h"
#include "device_fn/vortex_su.h"
#include "device_fn/vortex_sp.h"


#define TOPIC_ROOT      "vortex_device"

static const char *TAG = "VORTEX_DEV";

static Vortex_Dev *s_all[HUB_MAX_DEVICES];      // every device, any type
static int         s_all_count = 0;



/*===============================================================
 *                  NAMES
 *==============================================================*/

/**
 * @brief Topic name of a type (same as the devices and the server use)
 */
const char *vortex_dev_type_topic(DevType type)
{
    switch (type) {
        case DEV_VALVE:   return "wifi_valve";
        case DEV_SENSOR:  return "sensor_unit";
        case DEV_PLUG:    return "smart_plug";
        default:          return "unknown";
    }
}

/**
 * @brief Short name of a type (device list, web UI)
 */
const char *vortex_dev_type_name(DevType type)
{
    switch (type) {
        case DEV_VALVE:   return "valve";
        case DEV_SENSOR:  return "sensor";
        case DEV_PLUG:    return "plug";
        default:          return "unknown";
    }
}



/*===============================================================
 *                  JSON HELPERS
 *==============================================================*/

void dev_json_string(const cJSON *obj, const char *key, char *out, size_t out_size)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(item)) {
        strlcpy(out, item->valuestring, out_size);
    }
}

void dev_json_bool(const cJSON *obj, const char *key, bool *out)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item);
    }
}

void dev_json_int(const cJSON *obj, const char *key, int *out)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(item)) {
        *out = item->valueint;
    } else if (cJSON_IsString(item)) {
        *out = atoi(item->valuestring);             // the server sends some numbers as text
    }
}

void dev_json_float(const cJSON *obj, const char *key, float *out)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsNumber(item)) {
        *out = (float)item->valuedouble;
    } else if (cJSON_IsString(item)) {
        *out = (float)atof(item->valuestring);
    }
}



/*===============================================================
 *                  SETUP (boot)
 *==============================================================*/

/**
 * @brief Memory for a table (PSRAM first), zeroed
 */
static void *table_alloc(int count, size_t item_size)
{
    if (count == 0) {
        return NULL;
    }
    void *table = heap_caps_calloc(count, item_size, MALLOC_CAP_SPIRAM);
    if (table == NULL) {
        table = calloc(count, item_size);
    }
    return table;
}

/**
 * @brief Common part of a new object: id, type, topic, mutex
 */
static void dev_setup(Vortex_Dev *dev, DevType type, const char *device_id)
{
    dev->type = type;
    strlcpy(dev->device_id, device_id, sizeof(dev->device_id));
    snprintf(dev->topic_base, sizeof(dev->topic_base), "%s/%s/%s", TOPIC_ROOT, vortex_dev_type_topic(type), device_id);
    dev->mutex = xSemaphoreCreateMutex();

    s_all[s_all_count++] = dev;
    ESP_LOGI(TAG, "  %-6s %-14s %s", vortex_dev_type_name(type), device_id, dev->topic_base);
}

/**
 * @brief Create one object per device in hubDeviceList (call once at boot)
 */
void vortex_dev_init_all(void)
{
    /*----------------- 1. Count per type -----------------*/
    int counts[DEV_TYPE_COUNT] = { 0 };
    for (int i = 0; i < hubDeviceList.count; i++) {
        if (hubDeviceList.devices[i].type < DEV_TYPE_COUNT) {
            counts[hubDeviceList.devices[i].type]++;
        }
    }

    /*----------------- 2. Create the tables -----------------*/
    vortexValves  = table_alloc(counts[DEV_VALVE],  sizeof(Vortex_VA));
    vortexSensors = table_alloc(counts[DEV_SENSOR], sizeof(Vortex_SU));
    vortexPlugs   = table_alloc(counts[DEV_PLUG],   sizeof(Vortex_SP));

    vortexValveCount = vortexSensorCount = vortexPlugCount = 0;
    s_all_count = 0;

    /*----------------- 3. Fill one object per device -----------------*/
    ESP_LOGI(TAG, "Devices expected on this hub:");

    for (int i = 0; i < hubDeviceList.count; i++) {
        const HubDeviceEntry *entry = &hubDeviceList.devices[i];

        if (vortex_dev_find(entry->device_id) != NULL) {
            ESP_LOGW(TAG, "  %s listed twice, skipped", entry->device_id);
            continue;
        }

        switch (entry->type) {
            case DEV_VALVE:  dev_setup(&vortexValves[vortexValveCount++].dev,   DEV_VALVE,  entry->device_id); break;
            case DEV_SENSOR: dev_setup(&vortexSensors[vortexSensorCount++].dev, DEV_SENSOR, entry->device_id); break;
            case DEV_PLUG:   dev_setup(&vortexPlugs[vortexPlugCount++].dev,     DEV_PLUG,   entry->device_id); break;
            default:         ESP_LOGW(TAG, "  %s has an unknown type, skipped", entry->device_id); break;
        }
    }

    if (s_all_count == 0) {
        ESP_LOGW(TAG, "  (none: add devices in the hub config)");
    }
}



/*===============================================================
 *                  FIND
 *==============================================================*/

int vortex_dev_count(void)
{
    return s_all_count;
}

Vortex_Dev *vortex_dev_get(int index)
{
    return (index >= 0 && index < s_all_count) ? s_all[index] : NULL;
}

/**
 * @brief Device with this id (any type), NULL if not expected on this hub
 */
Vortex_Dev *vortex_dev_find(const char *device_id)
{
    for (int i = 0; i < s_all_count; i++) {
        if (strcmp(s_all[i]->device_id, device_id) == 0) {
            return s_all[i];
        }
    }
    return NULL;
}

/**
 * @brief Device of a topic "vortex_device/<type>/<id>/<sub>"
 *
 * @param sub_out  gets <sub> ("state_data", "cmd_data", ...)
 * @return NULL if the topic is not for a device expected on this hub
 */
Vortex_Dev *vortex_dev_from_topic(const char *topic, char *sub_out, size_t sub_size)
{
    char copy[128];
    char *save = NULL;

    strlcpy(copy, topic, sizeof(copy));

    char *root = strtok_r(copy, "/", &save);
    char *type = strtok_r(NULL, "/", &save);
    char *id   = strtok_r(NULL, "/", &save);
    char *sub  = strtok_r(NULL, "/", &save);

    if (root == NULL || type == NULL || id == NULL || sub == NULL || strcmp(root, TOPIC_ROOT) != 0) {
        return NULL;
    }

    Vortex_Dev *dev = vortex_dev_find(id);
    if (dev == NULL || strcmp(type, vortex_dev_type_topic(dev->type)) != 0) {
        return NULL;                                // not ours, or wrong type for this id
    }

    strlcpy(sub_out, sub, sub_size);
    return dev;
}



/*===============================================================
 *                  SAVE MESSAGES (local DB)
 *==============================================================*/

/**
 * @brief Message from the device (status / state_data / error) -> its variables
 */
void vortex_dev_save_device_msg(Vortex_Dev *dev, const char *sub, const char *json, int len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "%s: invalid JSON in %s", dev->device_id, sub);
        return;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    if (strcmp(sub, "status") == 0) {
        dev_json_string(root, "bin_version", dev->bin_version, sizeof(dev->bin_version));
    }

    switch (dev->type) {
        case DEV_VALVE:  va_save_device_msg((Vortex_VA *)dev, sub, root); break;
        case DEV_SENSOR: su_save_device_msg((Vortex_SU *)dev, sub, root); break;
        case DEV_PLUG:   sp_save_device_msg((Vortex_SP *)dev, sub, root); break;
        default: break;
    }

    if (!dev->online) {
        ESP_LOGI(TAG, "%s %s ONLINE", vortex_dev_type_name(dev->type), dev->device_id);
    }
    dev->online       = true;
    dev->last_seen_ms = esp_timer_get_time() / 1000;

    xSemaphoreGive(dev->mutex);
    cJSON_Delete(root);
}

/**
 * @brief Message from the server (cmd_data / control_data) -> its variables
 */
void vortex_dev_save_server_msg(Vortex_Dev *dev, const char *sub, const char *json, int len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "%s: invalid JSON in %s from server", dev->device_id, sub);
        return;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    switch (dev->type) {
        case DEV_VALVE:  va_save_server_msg((Vortex_VA *)dev, sub, root); break;
        case DEV_SENSOR: su_save_server_msg((Vortex_SU *)dev, sub, root); break;
        case DEV_PLUG:   sp_save_server_msg((Vortex_SP *)dev, sub, root); break;
        default: break;
    }

    xSemaphoreGive(dev->mutex);
    cJSON_Delete(root);
}



/*===============================================================
 *                  ONLINE / STATUS
 *==============================================================*/

/**
 * @brief Mark devices offline when nothing came for timeout_s
 */
void vortex_dev_check_timeouts(int timeout_s)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < s_all_count; i++) {
        Vortex_Dev *dev = s_all[i];

        xSemaphoreTake(dev->mutex, portMAX_DELAY);
        if (dev->online && (now - dev->last_seen_ms) > (int64_t)timeout_s * 1000) {
            dev->online = false;
            ESP_LOGW(TAG, "%s %s OFFLINE (no data for %d s)", vortex_dev_type_name(dev->type), dev->device_id, timeout_s);
        }
        xSemaphoreGive(dev->mutex);
    }
}

int vortex_dev_count_online(void)
{
    int online = 0;
    for (int i = 0; i < s_all_count; i++) {
        if (s_all[i]->online) online++;
    }
    return online;
}

/**
 * @brief One line per device (status log; the web UI shows the same later)
 */
void vortex_dev_print_all(void)
{
    int64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < s_all_count; i++) {
        Vortex_Dev *dev = s_all[i];
        char detail[96] = "";

        xSemaphoreTake(dev->mutex, portMAX_DELAY);

        switch (dev->type) {
            case DEV_VALVE:  va_describe((Vortex_VA *)dev, detail, sizeof(detail)); break;
            case DEV_SENSOR: su_describe((Vortex_SU *)dev, detail, sizeof(detail)); break;
            case DEV_PLUG:   sp_describe((Vortex_SP *)dev, detail, sizeof(detail)); break;
            default: break;
        }

        if (dev->last_seen_ms == 0) {
            ESP_LOGI(TAG, "  %-6s %-14s never seen", vortex_dev_type_name(dev->type), dev->device_id);
        } else {
            ESP_LOGI(TAG, "  %-6s %-14s %-7s last %llds ago  %s",
                     vortex_dev_type_name(dev->type), dev->device_id, dev->online ? "ONLINE" : "OFFLINE",
                     (long long)((now - dev->last_seen_ms) / 1000), detail);
        }

        xSemaphoreGive(dev->mutex);
    }
}