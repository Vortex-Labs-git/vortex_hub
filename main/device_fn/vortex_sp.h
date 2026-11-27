#ifndef VORTEX_SP_H
#define VORTEX_SP_H

#include <stddef.h>

#include "cJSON.h"
#include "global_fn/global_var.h"

/* Called by vortex_dev with the device mutex held */
void sp_save_device_msg(Vortex_SP *sp, const char *sub, const cJSON *root);
void sp_save_server_msg(Vortex_SP *sp, const char *sub, const cJSON *root);
void sp_describe(const Vortex_SP *sp, char *out, size_t out_size);

#endif // VORTEX_SP_H