#ifndef VORTEX_SU_H
#define VORTEX_SU_H

#include <stddef.h>

#include "cJSON.h"
#include "global_fn/global_var.h"

/* Called by vortex_dev with the device mutex held */
void su_save_device_msg(Vortex_SU *su, const char *sub, const cJSON *root);
void su_save_server_msg(Vortex_SU *su, const char *sub, const cJSON *root);
void su_describe(const Vortex_SU *su, char *out, size_t out_size);

#endif // VORTEX_SU_H