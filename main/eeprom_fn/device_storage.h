#ifndef DEVICE_STORAGE_H
#define DEVICE_STORAGE_H

#include "esp_err.h"
#include "global_fn/global_var.h"


esp_err_t device_storage_load(void);
esp_err_t device_storage_save(void);
void      device_storage_restore_default(void);

#endif /* DEVICE_STORAGE_H */