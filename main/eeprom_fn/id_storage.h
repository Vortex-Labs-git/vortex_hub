#ifndef ID_STORAGE_H
#define ID_STORAGE_H

#include "esp_err.h"
#include "global_fn/global_var.h"


esp_err_t id_storage_load(void);
esp_err_t id_storage_save(void);
void      id_storage_restore_default(void);

#endif /* ID_STORAGE_H */