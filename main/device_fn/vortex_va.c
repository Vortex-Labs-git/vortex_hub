/**
 * @file vortex_va.c
 * @brief Valve messages -> valve variables (Vortex_VA)
 *
 * From the valve:
 *   status      {"status","bin_version"}
 *   state_data  {"get_controller":{schedule,sensor}, "get_valvedata":{angle,is_open,is_close},
 *                "get_limitdata":{is_open_limit,open_limit,is_close_limit,close_limit}}
 *   error       {"error"}
 *
 * From the server:
 *   cmd_data     {"set_controller":{schedule,sensor}, "valve_data":{name,set_angle,angle},
 *                 "ota_update":{version,url}}
 *   control_data {"schedule":{"schedule_info":[...]},
 *                 "sensor":{"sensor_rule":{...}, "sensor_data":{unit_id,sensor_id,...}}}
 *
 * All functions run with the device mutex held (vortex_dev takes it).
 */

#include <stdio.h>
#include <string.h>

#include "device_fn/vortex_va.h"
#include "device_fn/vortex_dev.h"



/*===============================================================
 *                  FROM THE VALVE
 *==============================================================*/

void va_save_device_msg(Vortex_VA *va, const char *sub, const cJSON *root)
{
    VA_DeviceData *d = &va->device;

    dev_json_string(root, "timestamp", d->timestamp, sizeof(d->timestamp));

    if (strcmp(sub, "state_data") == 0) {

        const cJSON *controller = cJSON_GetObjectItem(root, "get_controller");
        dev_json_bool(controller, "schedule", &d->schedule_control);
        dev_json_bool(controller, "sensor",   &d->sensor_control);

        const cJSON *valve = cJSON_GetObjectItem(root, "get_valvedata");
        dev_json_int (valve, "angle",    &d->angle);
        dev_json_bool(valve, "is_open",  &d->is_open);
        dev_json_bool(valve, "is_close", &d->is_close);

        const cJSON *limit = cJSON_GetObjectItem(root, "get_limitdata");
        dev_json_bool(limit, "is_open_limit",  &d->open_limit_available);
        dev_json_bool(limit, "open_limit",     &d->open_limit_click);
        dev_json_bool(limit, "is_close_limit", &d->close_limit_available);
        dev_json_bool(limit, "close_limit",    &d->close_limit_click);
    }
    else if (strcmp(sub, "error") == 0) {
        dev_json_string(root, "error", d->error_msg, sizeof(d->error_msg));
    }
}



/*===============================================================
 *                  FROM THE SERVER
 *==============================================================*/

void va_save_server_msg(Vortex_VA *va, const char *sub, const cJSON *root)
{
    VA_ServerData *s = &va->server;

    if (strcmp(sub, "cmd_data") == 0) {

        const cJSON *controller = cJSON_GetObjectItem(root, "set_controller");
        dev_json_bool(controller, "schedule", &s->schedule_control);
        dev_json_bool(controller, "sensor",   &s->sensor_control);

        const cJSON *valve = cJSON_GetObjectItem(root, "valve_data");
        dev_json_string(valve, "name",      s->name, sizeof(s->name));
        dev_json_bool  (valve, "set_angle", &s->set_angle);
        dev_json_int   (valve, "angle",     &s->angle);

        const cJSON *ota = cJSON_GetObjectItem(root, "ota_update");
        dev_json_string(ota, "version", s->ota_version, sizeof(s->ota_version));
    }
    else if (strcmp(sub, "control_data") == 0) {

        const cJSON *schedule = cJSON_GetObjectItem(root, "schedule");
        const cJSON *info     = cJSON_GetObjectItem(schedule, "schedule_info");
        if (cJSON_IsArray(info)) {
            s->schedule_count = cJSON_GetArraySize(info);
        }

        const cJSON *sensor = cJSON_GetObjectItem(root, "sensor");
        const cJSON *rule   = cJSON_GetObjectItem(sensor, "sensor_rule");
        if (cJSON_IsObject(rule)) {
            s->sensor_rule_count = cJSON_GetArraySize(rule);
        }

        const cJSON *link = cJSON_GetObjectItem(sensor, "sensor_data");
        dev_json_string(link, "unit_id",   s->sensor_unit_id, sizeof(s->sensor_unit_id));
        dev_json_string(link, "sensor_id", s->sensor_id,      sizeof(s->sensor_id));
    }
}



/*===============================================================
 *                  STATUS LINE
 *==============================================================*/

void va_describe(const Vortex_VA *va, char *out, size_t out_size)
{
    const VA_DeviceData *d = &va->device;
    const char *mode = d->sensor_control ? "sensor" : d->schedule_control ? "schedule" : "manual";

    snprintf(out, out_size, "angle %d %s, %s%s%s",
             d->angle, d->is_open ? "(open)" : d->is_close ? "(closed)" : "",
             mode, d->error_msg[0] ? ", error: " : "", d->error_msg);
}