#ifndef VORTEX_DEV_H
#define VORTEX_DEV_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "global_fn/global_var.h"


/* Setup (boot) */
void        vortex_dev_init_all(void);

/* Find */
int         vortex_dev_count(void);
Vortex_Dev *vortex_dev_get(int index);
Vortex_Dev *vortex_dev_find(const char *device_id);
Vortex_Dev *vortex_dev_from_topic(const char *topic, char *sub_out, size_t sub_size);

/* Save a message into the device's variables */
void        vortex_dev_save_device_msg(Vortex_Dev *dev, const char *sub, const char *json, int len);
void        vortex_dev_save_server_msg(Vortex_Dev *dev, const char *sub, const char *json, int len);

/* Online / status */
void        vortex_dev_check_timeouts(int timeout_s);
int         vortex_dev_count_online(void);
void        vortex_dev_print_all(void);

/* Names */
const char *vortex_dev_type_topic(DevType type);
const char *vortex_dev_type_name(DevType type);

/* JSON helpers for the type files (a missing key keeps the old value) */
void        dev_json_string(const cJSON *obj, const char *key, char *out, size_t out_size);
void        dev_json_bool(const cJSON *obj, const char *key, bool *out);
void        dev_json_int(const cJSON *obj, const char *key, int *out);
void        dev_json_float(const cJSON *obj, const char *key, float *out);

#endif // VORTEX_DEV_H