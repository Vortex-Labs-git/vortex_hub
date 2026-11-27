/**
 * @file main.c
 * @brief Vortex Hub (ESP32-S3) — step 1c: devices <-> hub <-> server
 *
 * Devices (valve, sensor unit, plug) join the hub's own Wi-Fi
 * ("Vortex_VHUB202611001") and use the hub's local MQTT broker.
 * The hub keeps a variable set per expected device (local DB) and
 * forwards device data to the server and server commands to the devices.
 * NAT still gives the devices internet for OTA and SNTP.
 *
 * Start-up:
 *  1. NVS, netif, event loop
 *  2. Hub identity (hub ID = Wi-Fi name)
 *  3. Wi-Fi settings (router, hub Wi-Fi password)
 *  4. Device list -> one variable set + topics per device
 *  5. Hub Wi-Fi (AP) + router (STA) + NAT
 *  6. MQTT bridge, local broker, local client, server client
 *  7. Status line every 30 s
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "global_fn/global_var.h"
#include "eeprom_fn/id_storage.h"
#include "eeprom_fn/wifi_storage.h"
#include "eeprom_fn/device_storage.h"
#include "device_fn/vortex_dev.h"
#include "wifi_fn/wifi_hub.h"
#include "broker_fn/local_broker.h"
#include "mqtt_fn/mqtt_bridge.h"
#include "mqtt_fn/local_client.h"
#include "mqtt_fn/cloud_client.h"


#define STATUS_PERIOD_MS    30000

static const char *TAG_MAIN = "HUB_MAIN";



/* ======================================================================== */
/* ================================ MAIN ================================== */
/* ======================================================================== */

void app_main(void)
{
    // NVS Init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


#if CONFIG_HUB_ID_RESET
    id_storage_restore_default();
#endif
    id_storage_load();

#if CONFIG_HUB_WIFI_RESET
    wifi_storage_restore_default();
#endif
    wifi_storage_load();

#if CONFIG_HUB_DEVICE_RESET
    device_storage_restore_default();
#endif
    device_storage_load();

    vortex_dev_init_all();


    wifi_hub_start();

    mqtt_bridge_start();
    local_broker_start();
    local_client_start();
    cloud_client_start();


    /* Status line (the web UI shows the same later) */
    while (1) {
        char router_ip[16];
        wifi_hub_router_ip(router_ip, sizeof(router_ip));

        ESP_LOGI(TAG_MAIN, "[%s] router: %s %s | server: %s | Wi-Fi clients: %d | devices online: %d/%d",
                 hubIdentity.hub_id,
                 wifi_hub_router_connected() ? "connected" : "not connected",
                 router_ip,
                 cloud_client_connected() ? "connected" : "not connected",
                 wifi_hub_client_count(),
                 vortex_dev_count_online(),
                 vortex_dev_count());

        vortex_dev_print_all();

        vTaskDelay(pdMS_TO_TICKS(STATUS_PERIOD_MS));
    }
}