/**
 * @file wifi_storage.c
 * @brief NVS-based hub Wi-Fi settings storage
 *
 * This module:
 *  - Loads the router SSID / password and the hub Wi-Fi password from NVS
 *  - Saves them when the user changes them (web UI, step 1d)
 *  - Restores the defaults from menuconfig
 *
 * Stored as a binary blob (HubWifi structure).
 */

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi_storage.h"


/* ======================================================================== */
/* ========================== CONFIGURATION =============================== */
/* ======================================================================== */

/**
 * @brief Defaults from menuconfig
 */
#define DEFAULT_ROUTER_SSID   CONFIG_HUB_ROUTER_SSID
#define DEFAULT_ROUTER_PASS   CONFIG_HUB_ROUTER_PASSWORD
#define DEFAULT_AP_PASS       CONFIG_HUB_AP_PASSWORD

#define WIFI_NVS_NAMESPACE    "wifi_cfg"
#define WIFI_NVS_KEY          "hub_wifi"

#define AP_PASS_MIN_LEN       8         // WPA2 needs at least 8 characters

static const char *TAG_WIFI = "wifi_storage";



/* ======================================================================== */
/* ========================== LOAD WIFI FROM NVS ========================== */
/* ======================================================================== */

/**
 * @brief Load the Wi-Fi settings from NVS into hubWifi
 *
 * Nothing stored (or the struct changed size) -> restore defaults.
 */
esp_err_t wifi_storage_load(void)
{
    nvs_handle_t handle;
    size_t size = sizeof(HubWifi);

    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_blob(handle, WIFI_NVS_KEY, &hubWifi, &size);
        nvs_close(handle);
    }

    if (err != ESP_OK || size != sizeof(HubWifi)) {
        ESP_LOGW(TAG_WIFI, "No stored Wi-Fi settings, using defaults");
        wifi_storage_restore_default();
        return ESP_OK;
    }

    ESP_LOGI(TAG_WIFI, "Wi-Fi loaded (router=\"%s\", set_wifi=%d)", hubWifi.router_ssid, hubWifi.set_wifi);
    return ESP_OK;
}



/* ======================================================================== */
/* ========================== SAVE WIFI TO NVS ============================ */
/* ======================================================================== */

/**
 * @brief Save hubWifi to NVS
 *
 * @return ESP_ERR_INVALID_ARG if the hub Wi-Fi password is too short
 */
esp_err_t wifi_storage_save(void)
{
    nvs_handle_t handle;

    if (strlen(hubWifi.ap_pass) < AP_PASS_MIN_LEN) {
        ESP_LOGE(TAG_WIFI, "Hub Wi-Fi password must be at least %d characters", AP_PASS_MIN_LEN);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WIFI, "Failed to open NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, WIFI_NVS_KEY, &hubWifi, sizeof(HubWifi));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_WIFI, "Failed to save Wi-Fi settings (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG_WIFI, "Wi-Fi settings saved");
    }
    return err;
}



/* ======================================================================== */
/* ====================== RESTORE DEFAULT WIFI ============================ */
/* ======================================================================== */

/**
 * @brief Restore the Wi-Fi settings from menuconfig and save them
 *
 * Used when:
 *  - First boot (nothing stored)
 *  - CONFIG_HUB_WIFI_RESET is enabled
 */
void wifi_storage_restore_default(void)
{
    memset(&hubWifi, 0, sizeof(HubWifi));

    strlcpy(hubWifi.router_ssid, DEFAULT_ROUTER_SSID, sizeof(hubWifi.router_ssid));
    strlcpy(hubWifi.router_pass, DEFAULT_ROUTER_PASS, sizeof(hubWifi.router_pass));
    strlcpy(hubWifi.ap_pass,     DEFAULT_AP_PASS,     sizeof(hubWifi.ap_pass));
    hubWifi.set_wifi = false;

    wifi_storage_save();

    ESP_LOGI(TAG_WIFI, "Wi-Fi restored to default (router=\"%s\")", hubWifi.router_ssid);
}