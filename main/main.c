/**
 * @file main.c
 * @brief Vortex Hub — ESP-NOW link test example (ESP32-S3)
 *
 * What it does:
 *  - Runs ESP-NOW on a fixed channel (no router, no MQTT)
 *  - Accepts devices that send a CALLX signed with the pair key
 *  - Answers CALLX_ACK + TIME_SYNC, sends TIME_SYNC every hour
 *  - Prints every status / state_data / error as "MQTT topic + raw JSON"
 *  - Answers REJOIN to data from devices it does not know
 *  - BOOT button: sends a test cmd_data (angle 90 / 0) to every valve
 *
 * Start-up:
 *  1. NVS, netif, event loop
 *  2. Test clock (build time)
 *  3. WiFi STA (not connected) on HUB_CHANNEL
 *  4. ESP-NOW link + device table
 *  5. Link task and test button task
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "espnow_fn/espnow_link.h"
#include "hub_fn/device_table.h"
#include "hub_fn/hub_link.h"
#include "hub_fn/hub_test.h"
#include "time_fn/hub_time.h"


static const char *TAG_MAIN = "HUB_MAIN";



/* ======================================================================== */
/* ============================ WIFI FOR ESP-NOW ========================== */
/* ======================================================================== */

/**
 * @brief WiFi STA, never connected, fixed on HUB_CHANNEL
 *
 * The real hub joins the router here, and the channel follows the router.
 */
static void wifi_init_espnow(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));            // never miss an ESP-NOW frame
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(CONFIG_HUB_TX_POWER));    // lower TX current peaks (brownout)
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_HUB_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

/**
 * @brief Print what to put in the valve's menuconfig
 */
static void print_banner(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    ESP_LOGW(TAG_MAIN, "==================================================");
    ESP_LOGW(TAG_MAIN, " VORTEX HUB EXAMPLE   id: %s", CONFIG_HUB_ID);
    ESP_LOGW(TAG_MAIN, " Valve menuconfig -> Vortex Hub Link -> HUB Configuration:");
    ESP_LOGW(TAG_MAIN, "   HUB_DEFAULT_MAC      = " MACSTR, MAC2STR(mac));
    ESP_LOGW(TAG_MAIN, "   HUB_DEFAULT_PAIR_KEY = %s", CONFIG_HUB_PAIR_KEY);
    ESP_LOGW(TAG_MAIN, " Channel: %d   TX power: %.2f dBm", CONFIG_HUB_CHANNEL, CONFIG_HUB_TX_POWER * 0.25);
    ESP_LOGW(TAG_MAIN, "==================================================");
}



/* ======================================================================== */
/* ================================ MAIN ================================== */
/* ======================================================================== */

void app_main(void)
{
    // NVS Init (WiFi driver needs it)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    hub_time_init();

    wifi_init_espnow();

    device_table_init();

    espnow_link_config_t espnow_config = {
        .dev_type   = ESPNOW_DEV_HUB,
        .on_message = hub_link_on_message,
    };
    ESP_ERROR_CHECK(espnow_link_init(&espnow_config));

    hub_link_start();
    hub_test_start();

    print_banner();
}
