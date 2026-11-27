/**
 * @file device_storage.c
 * @brief NVS storage of the devices expected on this hub
 *
 * This module:
 *  - Loads the device list (type + device_id) from NVS at boot
 *  - Saves it when the user changes it (web UI, step 1d; the hub restarts after)
 *  - Restores the default list from menuconfig
 *
 * Stored as one binary blob (HubDeviceList structure).
 *
 * menuconfig default, until the web UI exists:
 *   "valve:VA202611301;sensor:SU202611001;plug:SP202611001"
 */

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "device_storage.h"


/* ======================================================================== */
/* ========================== CONFIGURATION =============================== */
/* ======================================================================== */

#define DEFAULT_DEVICE_LIST     CONFIG_HUB_DEVICE_LIST

#define DEV_NVS_NAMESPACE       "dev_cfg"
#define DEV_NVS_KEY             "devices"

static const char *TAG_DEV = "device_storage";



/* ======================================================================== */
/* =========================== LOAD FROM NVS ============================== */
/* ======================================================================== */

/**
 * @brief Load the device list from NVS into hubDeviceList
 *
 * Nothing stored (or the struct changed size) -> restore defaults.
 */
esp_err_t device_storage_load(void)
{
    nvs_handle_t handle;
    size_t size = sizeof(HubDeviceList);

    esp_err_t err = nvs_open(DEV_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_blob(handle, DEV_NVS_KEY, &hubDeviceList, &size);
        nvs_close(handle);
    }

    if (err != ESP_OK || size != sizeof(HubDeviceList) || hubDeviceList.count > HUB_MAX_DEVICES) {
        ESP_LOGW(TAG_DEV, "No stored device list, using defaults");
        // device_storage_restore_default();
        return ESP_OK;
    }

    ESP_LOGI(TAG_DEV, "Device list loaded (%d devices)", hubDeviceList.count);
    return ESP_OK;
}



/* ======================================================================== */
/* ============================ SAVE TO NVS =============================== */
/* ======================================================================== */

/**
 * @brief Save hubDeviceList to NVS
 */
esp_err_t device_storage_save(void)
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(DEV_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DEV, "Failed to open NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, DEV_NVS_KEY, &hubDeviceList, sizeof(HubDeviceList));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_DEV, "Failed to save device list (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG_DEV, "Device list saved (%d devices)", hubDeviceList.count);
    }
    return err;
}



// /* ======================================================================== */
// /* ======================= RESTORE DEFAULT LIST =========================== */
// /* ======================================================================== */

// /**
//  * @brief "valve" / "sensor" / "plug" -> DevType (DEV_TYPE_COUNT if unknown)
//  */
// static DevType type_from_name(const char *name)
// {
//     if (strcmp(name, "valve")  == 0) return DEV_VALVE;
//     if (strcmp(name, "sensor") == 0) return DEV_SENSOR;
//     if (strcmp(name, "plug")   == 0) return DEV_PLUG;
//     return DEV_TYPE_COUNT;
// }

// /**
//  * @brief Fill hubDeviceList from the menuconfig string and save it
//  *
//  * Format: "type:id;type:id;..."  e.g. "valve:VA202611301;sensor:SU202611001"
//  */
// void device_storage_restore_default(void)
// {
//     char list[256];
//     char *save = NULL;

//     memset(&hubDeviceList, 0, sizeof(HubDeviceList));
//     strlcpy(list, DEFAULT_DEVICE_LIST, sizeof(list));

//     for (char *item = strtok_r(list, ";", &save); item != NULL; item = strtok_r(NULL, ";", &save)) {

//         char *colon = strchr(item, ':');
//         if (colon == NULL) {
//             ESP_LOGW(TAG_DEV, "Skipped \"%s\" (format is type:id)", item);
//             continue;
//         }
//         *colon = '\0';

//         DevType type = type_from_name(item);
//         const char *id = colon + 1;

//         if (type == DEV_TYPE_COUNT || strlen(id) == 0) {
//             ESP_LOGW(TAG_DEV, "Skipped \"%s:%s\" (type must be valve, sensor or plug)", item, id);
//             continue;
//         }
//         if (hubDeviceList.count >= HUB_MAX_DEVICES) {
//             ESP_LOGW(TAG_DEV, "More than %d devices, the rest are skipped", HUB_MAX_DEVICES);
//             break;
//         }

//         HubDeviceEntry *entry = &hubDeviceList.devices[hubDeviceList.count++];
//         entry->type = (uint8_t)type;
//         strlcpy(entry->device_id, id, sizeof(entry->device_id));
//     }

//     device_storage_save();

//     ESP_LOGI(TAG_DEV, "Device list restored to default (%d devices)", hubDeviceList.count);
// }