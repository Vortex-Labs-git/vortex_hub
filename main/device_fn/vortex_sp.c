/**
 * @file vortex_sp.c
 * @brief Smart plug messages -> plug variables (Vortex_SP)
 *
 * Every message has one block per socket: "base_A" and "base_B".
 *
 * From the plug:
 *   status      {"status"}
 *   state_data  {"base_X":{"get_controller":{schedule,sensor}, "get_data":{name,state}, "get_current"}}
 *   error       {"error"}
 *
 * From the server:
 *   cmd_data     {"base_X":{"set_controller":{schedule,sensor}, "set_data":{name,state}},
 *                 "ota_update":{version,url}}
 *   control_data {"base_X":{"schedule":{"schedule_info":[...]},
 *                           "sensor":{"sensor_rule":{...}, "sensor_data":{unit_id,sensor_id,...}}}}
 *
 * All functions run with the device mutex held (vortex_dev takes it).
 */

#include <stdio.h>
#include <string.h>

#include "device_fn/vortex_sp.h"
#include "device_fn/vortex_dev.h"


static const char *BASE_KEY[SP_BASES] = { "base_A", "base_B" };



/*===============================================================
 *                  FROM THE PLUG
 *==============================================================*/

void sp_save_device_msg(Vortex_SP *sp, const char *sub, const cJSON *root)
{
    SP_DeviceData *d = &sp->device;

    dev_json_string(root, "timestamp", d->timestamp, sizeof(d->timestamp));

    if (strcmp(sub, "state_data") == 0) {

        for (int b = 0; b < SP_BASES; b++) {
            const cJSON *base = cJSON_GetObjectItem(root, BASE_KEY[b]);
            if (!cJSON_IsObject(base)) continue;

            const cJSON *controller = cJSON_GetObjectItem(base, "get_controller");
            dev_json_bool(controller, "schedule", &d->base[b].schedule_control);
            dev_json_bool(controller, "sensor",   &d->base[b].sensor_control);

            const cJSON *data = cJSON_GetObjectItem(base, "get_data");
            dev_json_string(data, "name",  d->base[b].name, sizeof(d->base[b].name));
            dev_json_bool  (data, "state", &d->base[b].state);

            dev_json_int(base, "get_current", &d->base[b].current);
        }
    }
    else if (strcmp(sub, "error") == 0) {
        dev_json_string(root, "error", d->error_msg, sizeof(d->error_msg));
    }
}



/*===============================================================
 *                  FROM THE SERVER
 *==============================================================*/

void sp_save_server_msg(Vortex_SP *sp, const char *sub, const cJSON *root)
{
    SP_ServerData *s = &sp->server;

    for (int b = 0; b < SP_BASES; b++) {
        const cJSON *base = cJSON_GetObjectItem(root, BASE_KEY[b]);
        if (!cJSON_IsObject(base)) continue;

        SP_BaseCommand *cmd = &s->base[b];

        if (strcmp(sub, "cmd_data") == 0) {
            const cJSON *controller = cJSON_GetObjectItem(base, "set_controller");
            dev_json_bool(controller, "schedule", &cmd->schedule_control);
            dev_json_bool(controller, "sensor",   &cmd->sensor_control);

            const cJSON *data = cJSON_GetObjectItem(base, "set_data");
            dev_json_string(data, "name",  cmd->name, sizeof(cmd->name));
            dev_json_bool  (data, "state", &cmd->state);
        }
        else if (strcmp(sub, "control_data") == 0) {
            const cJSON *schedule = cJSON_GetObjectItem(base, "schedule");
            const cJSON *info     = cJSON_GetObjectItem(schedule, "schedule_info");
            if (cJSON_IsArray(info)) {
                cmd->schedule_count = cJSON_GetArraySize(info);
            }

            const cJSON *sensor = cJSON_GetObjectItem(base, "sensor");
            const cJSON *rule   = cJSON_GetObjectItem(sensor, "sensor_rule");
            if (cJSON_IsObject(rule)) {
                cmd->sensor_rule_count = cJSON_GetArraySize(rule);
            }

            const cJSON *link = cJSON_GetObjectItem(sensor, "sensor_data");
            dev_json_string(link, "unit_id",   cmd->sensor_unit_id, sizeof(cmd->sensor_unit_id));
            dev_json_string(link, "sensor_id", cmd->sensor_id,      sizeof(cmd->sensor_id));
        }
    }

    if (strcmp(sub, "cmd_data") == 0) {
        const cJSON *ota = cJSON_GetObjectItem(root, "ota_update");
        dev_json_string(ota, "version", s->ota_version, sizeof(s->ota_version));
    }
}



/*===============================================================
 *                  STATUS LINE
 *==============================================================*/

void sp_describe(const Vortex_SP *sp, char *out, size_t out_size)
{
    const SP_DeviceData *d = &sp->device;

    snprintf(out, out_size, "A %s %dmA, B %s %dmA%s%s",
             d->base[0].state ? "ON" : "OFF", d->base[0].current,
             d->base[1].state ? "ON" : "OFF", d->base[1].current,
             d->error_msg[0] ? ", error: " : "", d->error_msg);
}