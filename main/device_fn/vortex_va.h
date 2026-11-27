#ifndef VORTEX_VA_H
#define VORTEX_VA_H

#include <stddef.h>

#include "cJSON.h"
#include "global_fn/global_var.h"

/* Called by vortex_dev with the device mutex held */
void va_save_device_msg(Vortex_VA *va, const char *sub, const cJSON *root);
void va_save_server_msg(Vortex_VA *va, const char *sub, const cJSON *root);
void va_describe(const Vortex_VA *va, char *out, size_t out_size);

#endif // VORTEX_VA_H