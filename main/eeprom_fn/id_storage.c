/**
 * @file id_storage.c
 * @brief NVS-based hub identity storage (hub_id + Wi-Fi name)
 *
 * This module:
 *  - Loads the hub identity from NVS at boot
 *  - Seeds it from menuconfig on a fresh hub (empty NVS)
 *  - Saves it to NVS
 *  - Restores the default identity from menuconfig (factory reset)
 *
 * The hub's own Wi-Fi name is the hub ID itself ("VHUB2026001"), so
 * devices know they joined a hub when the SSID starts with "VHUB".
 */

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "id_storage.h"


/* ======================================================================== */
/* ============================ CONFIGURATION ============================= */
/* ======================================================================== */

/* Default hub ID from menuconfig (the seed value for a fresh hub) */
#define DEFAULT_HUB_ID        CONFIG_HUB_ID

/* Every hub ID must start with this (devices check the SSID for it) */
#define HUB_ID_PREFIX         "VHUB"

#define ID_NVS_NAMESPACE      "id_cfg"
#define ID_NVS_KEY            "identity"

static const char *TAG_ID = "id_storage";



/* ======================================================================== */
/* ========================== LOAD FROM NVS =============================== */
/* ======================================================================== */

/**
 * @brief Load the hub identity from NVS into hubIdentity
 *
 * Nothing stored, or the stored ID is empty -> seed from menuconfig.
 */
esp_err_t id_storage_load(void) {
    nvs_handle_t handle;
    esp_err_t err;
    size_t size = sizeof(HubIdentity);

    err = nvs_open(ID_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_ID, "No stored identity, seeding from menuconfig");
        id_storage_restore_default();
        return ESP_OK;
    }

    err = nvs_get_blob(handle, ID_NVS_KEY, &hubIdentity, &size);
    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(HubIdentity) || strlen(hubIdentity.hub_id) == 0) {
        ESP_LOGW(TAG_ID, "Stored identity missing or invalid, seeding defaults");
        id_storage_restore_default();
        return ESP_OK;
    }

    ESP_LOGI(TAG_ID, "Identity loaded (id=%s, ap_ssid=%s)", hubIdentity.hub_id, hubIdentity.ap_ssid);
    return ESP_OK;
}



/* ======================================================================== */
/* ============================ SAVE TO NVS =============================== */
/* ======================================================================== */

/**
 * @brief Save hubIdentity to NVS as a blob
 */
esp_err_t id_storage_save(void) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(ID_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_ID, "Failed to open NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, ID_NVS_KEY, &hubIdentity, sizeof(HubIdentity));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_ID, "Failed to save identity (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG_ID, "Identity saved (id=%s)", hubIdentity.hub_id);
    }
    return err;
}



/* ======================================================================== */
/* ====================== RESTORE DEFAULT IDENTITY ======================= */
/* ======================================================================== */

/**
 * @brief Restore the identity from menuconfig and save it
 *
 *  1. Clear the structure
 *  2. Copy DEFAULT_HUB_ID (from menuconfig)
 *  3. Wi-Fi name = hub ID (fixed)
 *  4. Save to NVS
 */
void id_storage_restore_default(void) {
    memset(&hubIdentity, 0, sizeof(HubIdentity));

    strlcpy(hubIdentity.hub_id,  DEFAULT_HUB_ID, sizeof(hubIdentity.hub_id));
    strlcpy(hubIdentity.ap_ssid, DEFAULT_HUB_ID, sizeof(hubIdentity.ap_ssid));

    if (strncmp(hubIdentity.hub_id, HUB_ID_PREFIX, strlen(HUB_ID_PREFIX)) != 0) {
        ESP_LOGE(TAG_ID, "HUB_ID \"%s\" does not start with \"%s\": devices will not see it as a hub",
                 hubIdentity.hub_id, HUB_ID_PREFIX);
    }

    id_storage_save();

    ESP_LOGI(TAG_ID, "Identity restored to default (id=%s)", hubIdentity.hub_id);
}