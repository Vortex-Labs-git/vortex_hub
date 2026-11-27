/**
 * @file local_broker.c
 * @brief Local MQTT broker for the devices on the hub Wi-Fi
 *
 *   Valve / Sensor unit / Plug  ──mqtt://192.168.4.1:1883──►  LOCAL BROKER
 *   (SSID "Vortex_VHUB...")                                    (mosquitto)
 *
 * Devices that joined the hub Wi-Fi publish and subscribe here with the
 * same topics and JSON as on the server:
 *   vortex_device/<type>/<device_id>/status | state_data | error     (device -> hub)
 *   vortex_device/<type>/<device_id>/cmd_data | control_data         (hub -> device)
 *
 * The broker listens only on the hub Wi-Fi (not on the router LAN).
 *
 * Callbacks (run inside the broker task, keep them short):
 *   on_client_connect  -> accept devices in the hub's device list (client id = device_id)
 *                         and the hub's own bridge client
 *   on_broker_message  -> every message: copied to the MQTT bridge queue
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mosq_broker.h"

#include "local_broker.h"
#include "wifi_fn/wifi_hub.h"
#include "mqtt_fn/mqtt_bridge.h"
#include "mqtt_fn/local_client.h"
#include "device_fn/vortex_dev.h"


#define BROKER_TASK_STACK   (32 * 1024)     // same as the mosquitto example
#define BROKER_TASK_PRIO    5
#define LOG_DATA_MAX        160             // characters of a message shown in the log

static const char *TAG = "LOCAL_BROKER";

static char s_listen_ip[16];



/*===============================================================
 *                  BROKER CALLBACKS (broker task)
 *==============================================================*/

/**
 * @brief A client connects (devices use their device_id as client id)
 *
 * @return 0 = accept, 1 = reject
 */
static int on_client_connect(const char *client_id, const char *username, const char *password, int password_len)
{
    if (client_id == NULL) {
        return 1;
    }

    if (strcmp(client_id, local_client_id()) == 0) {
        return 0;                                       // the hub's own bridge client
    }

    if (vortex_dev_find(client_id) != NULL) {
        ESP_LOGI(TAG, "Device connected: %s", client_id);
        return 0;
    }

#if CONFIG_HUB_BROKER_ALLOW_UNKNOWN
    ESP_LOGW(TAG, "Unknown client %s accepted (HUB_BROKER_ALLOW_UNKNOWN)", client_id);
    return 0;
#else
    ESP_LOGW(TAG, "Unknown client %s rejected (not in the device list)", client_id);
    return 1;
#endif
}

/**
 * @brief Every message that passes the broker
 *
 * data is NOT NUL-terminated: use len.
 */
static void on_broker_message(char *client, char *topic, char *data, int len, int qos, int retain)
{
    /* Device messages go to the bridge; the hub's own commands to the devices do not */
    if (client == NULL || strcmp(client, local_client_id()) != 0) {
        mqtt_bridge_from_device(topic, data, len);
    }

#if CONFIG_HUB_BROKER_LOG
    int shown = (len < LOG_DATA_MAX) ? len : LOG_DATA_MAX;
    ESP_LOGI(TAG, "%s -> %s (%d B%s)\n%.*s%s",
             client ? client : "?", topic, len, retain ? ", retained" : "",
             shown, data, (len > shown) ? " ..." : "");
#endif
}



/*===============================================================
 *                  BROKER TASK
 *==============================================================*/

/**
 * @brief Run mosquitto (blocks while the broker runs)
 */
static void local_broker_task(void *pvParameters)
{
    struct mosq_broker_config config = {
        .host              = s_listen_ip,
        .port              = LOCAL_BROKER_PORT,
        .tls_cfg           = NULL,                  // plain MQTT inside the hub Wi-Fi
        .handle_message_cb = on_broker_message,
        .handle_connect_cb = on_client_connect,
    };

    ESP_LOGI(TAG, "Broker listening on mqtt://%s:%d", s_listen_ip, LOCAL_BROKER_PORT);

    int ret = mosq_broker_run(&config);

    ESP_LOGE(TAG, "Broker stopped (%d)", ret);
    vTaskDelete(NULL);
}

/**
 * @brief Start the broker on the hub Wi-Fi address (call after wifi_hub_start)
 */
void local_broker_start(void)
{
    wifi_hub_ap_ip(s_listen_ip, sizeof(s_listen_ip));

    if (xTaskCreate(local_broker_task, "local_broker", BROKER_TASK_STACK, NULL, BROKER_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create broker task");
    }
}