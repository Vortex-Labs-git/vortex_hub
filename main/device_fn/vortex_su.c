/**
 * @file vortex_su.c
 * @brief Sensor unit messages -> sensor unit variables (Vortex_SU)
 *
 * From the sensor unit:
 *   status      {"status"}
 *   state_data  {"no_sensors", "data":[{sensor_id,sensor_type,sensor_name,sensor_value}, ...]}
 *   error       {"error":[{sensor_id,error}, ...]}
 *
 * From the server:
 *   cmd_data    {"ota_update":{version,url}}
 *
 * All functions run with the device mutex held (vortex_dev takes it).
 */

#include <stdio.h>
#include <string.h>

#include "device_fn/vortex_su.h"
#include "device_fn/vortex_dev.h"



/*===============================================================
 *                  HELPERS
 *==============================================================*/

/**
 * @brief Sensor slot with this id (a new slot if not seen before), NULL if full
 */
static SU_Sensor *sensor_slot(SU_DeviceData *d, const char *sensor_id)
{
    for (int i = 0; i < d->sensor_count; i++) {
        if (strcmp(d->sensors[i].sensor_id, sensor_id) == 0) {
            return &d->sensors[i];
        }
    }
    if (d->sensor_count >= SU_MAX_SENSORS) {
        return NULL;
    }

    SU_Sensor *slot = &d->sensors[d->sensor_count++];
    memset(slot, 0, sizeof(*slot));
    strlcpy(slot->sensor_id, sensor_id, sizeof(slot->sensor_id));
    return slot;
}



/*===============================================================
 *                  FROM THE SENSOR UNIT
 *==============================================================*/

void su_save_device_msg(Vortex_SU *su, const char *sub, const cJSON *root)
{
    SU_DeviceData *d = &su->device;

    dev_json_string(root, "timestamp", d->timestamp, sizeof(d->timestamp));

    if (strcmp(sub, "state_data") == 0) {

        const cJSON *list = cJSON_GetObjectItem(root, "data");
        const cJSON *item = NULL;

        cJSON_ArrayForEach(item, list) {
            const cJSON *id = cJSON_GetObjectItem(item, "sensor_id");
            if (!cJSON_IsString(id)) continue;

            SU_Sensor *sensor = sensor_slot(d, id->valuestring);
            if (sensor == NULL) continue;

            dev_json_string(item, "sensor_type",  sensor->sensor_type, sizeof(sensor->sensor_type));
            dev_json_string(item, "sensor_name",  sensor->sensor_name, sizeof(sensor->sensor_name));
            dev_json_float (item, "sensor_value", &sensor->sensor_value);
        }
    }
    else if (strcmp(sub, "error") == 0) {

        /* Clear old errors, then take the current list */
        for (int i = 0; i < d->sensor_count; i++) {
            d->sensors[i].error[0] = '\0';
        }

        const cJSON *list = cJSON_GetObjectItem(root, "error");
        const cJSON *item = NULL;

        cJSON_ArrayForEach(item, list) {
            const cJSON *id = cJSON_GetObjectItem(item, "sensor_id");
            if (!cJSON_IsString(id)) continue;

            SU_Sensor *sensor = sensor_slot(d, id->valuestring);
            if (sensor != NULL) {
                dev_json_string(item, "error", sensor->error, sizeof(sensor->error));
            }
        }
    }
}



/*===============================================================
 *                  FROM THE SERVER
 *==============================================================*/

void su_save_server_msg(Vortex_SU *su, const char *sub, const cJSON *root)
{
    if (strcmp(sub, "cmd_data") == 0) {
        const cJSON *ota = cJSON_GetObjectItem(root, "ota_update");
        dev_json_string(ota, "version", su->server.ota_version, sizeof(su->server.ota_version));
    }
}



/*===============================================================
 *                  STATUS LINE
 *==============================================================*/

void su_describe(const Vortex_SU *su, char *out, size_t out_size)
{
    const SU_DeviceData *d = &su->device;
    size_t used = 0;

    out[0] = '\0';
    for (int i = 0; i < d->sensor_count && used < out_size; i++) {
        used += snprintf(out + used, out_size - used, "%s%s=%.1f%s",
                         i ? " " : "", d->sensors[i].sensor_id, d->sensors[i].sensor_value,
                         d->sensors[i].error[0] ? "(err)" : "");
    }
}