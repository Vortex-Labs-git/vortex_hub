/**
 * @file cloud_client.c
 * @brief Hub connection to the server MQTT broker (TLS)
 *
 * The hub talks to the server on behalf of its devices, with the SAME
 * topics the devices would use on their own:
 *
 *   publish    vortex_device/<type>/<id>/status | state_data | error
 *   subscribe  vortex_device/<type>/<id>/cmd_data | control_data
 *              (only for the devices in the hub's device list)
 *
 * The server (MQTT bridge, DB, web panel) does not change.
 * Client id = hub ID. esp-mqtt reconnects on its own when the
 * router or the internet comes back.
 */

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt_fn/cloud_client.h"
#include "mqtt_fn/mqtt_bridge.h"
#include "global_fn/global_var.h"
#include "device_fn/vortex_dev.h"


#define SERVER_BROKER_URI   CONFIG_HUB_SERVER_BROKER_URI
#define MAX_MQTT_PAYLOAD    4096        // same limit as the devices

static const char *TAG = "CLOUD_CLIENT";

static esp_mqtt_client_handle_t s_client    = NULL;
static volatile bool            s_connected = false;

/* Server CA certificate (same file as the devices) */
extern const uint8_t _binary_ca_cert_pem_start[];
extern const uint8_t _binary_ca_cert_pem_end[];



/*===============================================================
 *                  SUBSCRIBE (one set per device)
 *==============================================================*/

/**
 * @brief Subscribe to cmd_data + control_data of every device in the list
 */
static void subscribe_devices(void)
{
    char topic[96];

    for (int i = 0; i < vortex_dev_count(); i++) {
        Vortex_Dev *dev = vortex_dev_get(i);

        snprintf(topic, sizeof(topic), "%s/cmd_data", dev->topic_base);
        esp_mqtt_client_subscribe(s_client, topic, 1);

        snprintf(topic, sizeof(topic), "%s/control_data", dev->topic_base);
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }

    ESP_LOGI(TAG, "Subscribed for %d device(s)", vortex_dev_count());
}



/*===============================================================
 *                  MQTT EVENT HANDLER
 *==============================================================*/

static void cloud_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    /* A big message can arrive in pieces: put it back together */
    static char  topic[128];
    static char *rx_data  = NULL;
    static int   rx_len   = 0;
    static int   rx_count = 0;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to server broker");
            s_connected = true;
            subscribe_devices();
            break;

        case MQTT_EVENT_DISCONNECTED:
            if (s_connected) {
                ESP_LOGW(TAG, "Server broker lost (devices keep working locally)");
            }
            s_connected = false;
            break;

        case MQTT_EVENT_DATA:
            /*----------------- First piece: topic + buffer -----------------*/
            if (event->current_data_offset == 0) {
                free(rx_data);
                rx_data  = NULL;
                rx_count = 0;
                rx_len   = event->total_data_len;

                if (event->topic_len >= (int)sizeof(topic) || rx_len > MAX_MQTT_PAYLOAD) {
                    ESP_LOGW(TAG, "Message too large, dropped");
                    break;
                }
                memcpy(topic, event->topic, event->topic_len);
                topic[event->topic_len] = '\0';

                rx_data = malloc(rx_len + 1);
                if (rx_data == NULL) {
                    ESP_LOGE(TAG, "No memory for %d-byte message", rx_len);
                    break;
                }
            }

            /*----------------- Every piece: copy -----------------*/
            if (rx_data == NULL) {
                break;
            }
            memcpy(rx_data + event->current_data_offset, event->data, event->data_len);
            rx_count += event->data_len;

            /*----------------- Last piece: to the bridge -----------------*/
            if (rx_count >= rx_len) {
                mqtt_bridge_from_server(topic, rx_data, rx_len);
                free(rx_data);
                rx_data = NULL;
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT error");
            break;

        default:
            break;
    }
}



/*===============================================================
 *                  PUBLISH
 *==============================================================*/

/**
 * @brief Publish to the server (skipped while not connected)
 *
 * @return true if handed to the MQTT client
 */
bool cloud_client_publish(const char *topic, const char *data, int len, bool retain)
{
    if (s_client == NULL || !s_connected) {
        return false;
    }
    return esp_mqtt_client_publish(s_client, topic, data, len, 1, retain) >= 0;
}

bool cloud_client_connected(void)
{
    return s_connected;
}



/*===============================================================
 *                  START
 *==============================================================*/

/**
 * @brief Start the server connection (call after vortex_dev_init_all)
 */
void cloud_client_start(void)
{
    esp_mqtt_client_config_t config = {
        .broker.address.uri              = SERVER_BROKER_URI,
        .broker.verification.certificate = (const char *)_binary_ca_cert_pem_start,
        .credentials.client_id           = hubIdentity.hub_id,
        .network.disable_auto_reconnect  = false,
        .session.keepalive               = 60,
        .buffer.size                     = MAX_MQTT_PAYLOAD,
    };

    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to create server MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, cloud_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "Server broker %s (client id %s)", SERVER_BROKER_URI, hubIdentity.hub_id);
}