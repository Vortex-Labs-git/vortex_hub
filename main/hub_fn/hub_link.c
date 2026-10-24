/**
 * @file hub_link.c
 * @brief Hub side of the ESP-NOW link
 *
 * Messages from devices:
 *   CALLX                     -> key check -> device_table -> CALLX_ACK + TIME_SYNC
 *   STATUS / STATE_DATA / ERR -> print the MQTT topic it would go to + raw JSON
 *   data from unknown MAC     -> REJOIN (device pairs again)
 *
 * Link task (every second):
 *   - marks silent devices OFFLINE
 *   - sends TIME_SYNC to every online device each HUB_TIME_SYNC_PERIOD_S
 *   - prints the device table every minute
 *
 * No WiFi / MQTT in this example: the "publish" is a log line.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "hub_fn/hub_link.h"
#include "hub_fn/device_table.h"
#include "time_fn/hub_time.h"
#include "espnow_fn/espnow_link.h"
#include "espnow_fn/espnow_pair.h"
#include "espnow_fn/espnow_util.h"


/*---------------------------------------------------------------
 * Configuration
 *--------------------------------------------------------------*/

#define HUB_ID                  CONFIG_HUB_ID
#define DEVICE_TIMEOUT_S        CONFIG_HUB_DEVICE_TIMEOUT_S
#define TIME_SYNC_PERIOD_S      CONFIG_HUB_TIME_SYNC_PERIOD_S
#define TABLE_PRINT_PERIOD_S    60

static const char *TAG = "HUB_LINK";

static uint8_t s_pair_key[ESPNOW_PAIR_KEY_LEN];



/*===============================================================
 *                  SEND HELPERS
 *==============================================================*/

/**
 * @brief Send the hub clock to one device
 */
static void send_time_sync(const uint8_t mac[6])
{
    espnow_time_sync_t sync = { .epoch_utc = hub_time_now() };

    esp_err_t err = espnow_send_msg(mac, ESPNOW_MSG_TIME_SYNC, &sync, sizeof(sync), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TIME_SYNC to " MACSTR " failed (%s)", MAC2STR(mac), esp_err_to_name(err));
    }
}

/**
 * @brief Tell an unknown device to pair again, then forget its peer entry
 */
static void send_rejoin(const uint8_t mac[6])
{
    ESP_LOGW(TAG, "Data from unknown " MACSTR " -> REJOIN", MAC2STR(mac));

    espnow_send_msg(mac, ESPNOW_MSG_REJOIN, NULL, 0, 0);
    espnow_link_del_peer(mac);
}



/*===============================================================
 *                  CALLX (pairing)
 *==============================================================*/

static void handle_callx(const uint8_t mac[6], const espnow_hdr_t *hdr, const uint8_t *data, size_t len, int rssi)
{
    espnow_callx_t callx;

    /*----------------- 1. Check size and key -----------------*/
    if (len != sizeof(callx)) {
        ESP_LOGW(TAG, "CALLX from " MACSTR " has wrong size %u", MAC2STR(mac), (unsigned)len);
        return;
    }
    memcpy(&callx, data, sizeof(callx));

    if (!espnow_pair_check_callx(&callx, hdr->dev_type, s_pair_key)) {
        ESP_LOGW(TAG, "CALLX from " MACSTR " (%.*s) REFUSED: wrong pair key",
                 MAC2STR(mac), ESPNOW_ID_LEN, callx.device_id);
        return;                                 // no answer: a wrong key learns nothing
    }

    /*----------------- 2. Add to the device table -----------------*/
    bool added = device_pair(mac, hdr->dev_type, &callx, rssi);

    /*----------------- 3. Answer CALLX_ACK -----------------*/
    espnow_callx_ack_t ack = { 0 };
    strlcpy(ack.hub_id, HUB_ID, sizeof(ack.hub_id));
    ack.channel = espnow_link_get_channel();
    ack.status  = added ? ESPNOW_CALLX_OK : ESPNOW_CALLX_HUB_FULL;
    memcpy(ack.nonce_echo, callx.nonce, ESPNOW_NONCE_LEN);
    espnow_pair_sign_callx_ack(&ack, s_pair_key);

    esp_err_t err = espnow_send_msg(mac, ESPNOW_MSG_CALLX_ACK, &ack, sizeof(ack), 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CALLX_ACK to " MACSTR " failed (%s)", MAC2STR(mac), esp_err_to_name(err));
        return;
    }

    if (!added) {
        ESP_LOGW(TAG, "Device table full, " MACSTR " refused", MAC2STR(mac));
        return;
    }

    /*----------------- 4. Give it the time right away -----------------*/
    send_time_sync(mac);
}



/*===============================================================
 *                  DEVICE DATA (status / state_data / error)
 *==============================================================*/

static void handle_device_data(const uint8_t mac[6], const espnow_hdr_t *hdr, const uint8_t *data, size_t len, int rssi)
{
    hub_device_t dev;

    if (!device_seen(mac, rssi, &dev)) {
        send_rejoin(mac);
        return;
    }

    const char *dev_topic = espnow_dev_topic_name(dev.dev_type);
    const char *sub_topic = espnow_topic_name(hdr->msg_type);

    /* Exactly what the real hub will publish to the broker */
    ESP_LOGI(TAG, "vortex_device/%s/%s/%s  (%u B, %u frames, rssi %d)\n%s",
             dev_topic ? dev_topic : "unknown", dev.device_id, sub_topic ? sub_topic : "?",
             (unsigned)len, hdr->frag_cnt, rssi, (const char *)data);
}



/*===============================================================
 *                  MESSAGE ROUTER (espnow_rx task)
 *==============================================================*/

/**
 * @brief Every complete message from ESP-NOW lands here
 */
void hub_link_on_message(const uint8_t mac[6], const espnow_hdr_t *hdr, const uint8_t *data, size_t len, int rssi)
{
    switch (hdr->msg_type) {

        case ESPNOW_MSG_CALLX:
            handle_callx(mac, hdr, data, len, rssi);
            break;

        case ESPNOW_MSG_STATUS:
        case ESPNOW_MSG_STATE_DATA:
        case ESPNOW_MSG_ERROR:
            handle_device_data(mac, hdr, data, len, rssi);
            break;

        default:
            ESP_LOGW(TAG, "Unexpected %s from " MACSTR " (%u B)",
                     espnow_msg_name(hdr->msg_type), MAC2STR(mac), (unsigned)len);
            break;
    }
}



/*===============================================================
 *                  LINK TASK
 *==============================================================*/

/**
 * @brief Offline check every second, TIME_SYNC every period, table every minute
 */
static void hub_link_task(void *pvParameters)
{
    uint32_t seconds = 0;
    hub_device_t online[CONFIG_HUB_MAX_DEVICES];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        seconds++;

        device_check_timeouts(DEVICE_TIMEOUT_S);

        /*----------------- Periodic TIME_SYNC -----------------*/
        if (seconds % TIME_SYNC_PERIOD_S == 0) {
            int count = device_get_online(online, CONFIG_HUB_MAX_DEVICES);
            for (int i = 0; i < count; i++) {
                send_time_sync(online[i].mac);
            }
            ESP_LOGI(TAG, "TIME_SYNC sent to %d device(s)", count);
        }

        /*----------------- Device table + link counters -----------------*/
        if (seconds % TABLE_PRINT_PERIOD_S == 0) {
            char now_text[32];
            espnow_stats_t stats;

            hub_time_string(now_text, sizeof(now_text));
            espnow_link_get_stats(&stats);

            ESP_LOGI(TAG, "---- %s | tx %lu ok / %lu fail | rx %lu ok / %lu bad / %lu dup ----",
                     now_text,
                     (unsigned long)stats.tx.msgs, (unsigned long)stats.tx.fail,
                     (unsigned long)stats.rx.msgs, (unsigned long)stats.rx.bad, (unsigned long)stats.rx.dup);
            device_print_all();
        }
    }
}



/*===============================================================
 *                  START
 *==============================================================*/

/**
 * @brief Load the pair key and start the link task
 */
void hub_link_start(void)
{
    if (!espnow_parse_key(CONFIG_HUB_PAIR_KEY, s_pair_key)) {
        ESP_LOGE(TAG, "HUB_PAIR_KEY must be 32 hex chars, no device can pair");
    }

    xTaskCreate(hub_link_task, "hub_link", 4096, NULL, 4, NULL);
}
