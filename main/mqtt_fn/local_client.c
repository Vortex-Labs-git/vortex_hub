/**
 * @file local_client.c
 * @brief Hub's own client on the local broker (sends server commands to devices)
 *
 *   server cmd_data / control_data -> bridge -> local_client -> LOCAL BROKER -> device
 *
 * The hub reads device messages straight from the broker (message
 * callback), so this client only publishes; it subscribes to nothing.
 * Client id = "<hub_id>_bridge" (the broker always accepts it).
 */

#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt_fn/local_client.h"
#include "broker_fn/local_broker.h"
#include "wifi_fn/wifi_hub.h"
#include "global_fn/global_var.h"


static const char *TAG = "LOCAL_CLIENT";

static esp_mqtt_client_handle_t s_client    = NULL;
static volatile bool            s_connected = false;
static char                     s_client_id[40];
static char                     s_uri[48];



/*===============================================================
 *                  MQTT EVENT HANDLER
 *==============================================================*/

static void local_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to local broker");
            s_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            break;

        default:
            break;
    }
}



/*===============================================================
 *                  PUBLISH
 *==============================================================*/

/**
 * @brief Publish on the local broker (to the devices)
 */
bool local_client_publish(const char *topic, const char *data, int len, bool retain)
{
    if (s_client == NULL || !s_connected) {
        return false;
    }
    return esp_mqtt_client_publish(s_client, topic, data, len, 1, retain) >= 0;
}

bool local_client_connected(void)
{
    return s_connected;
}

const char *local_client_id(void)
{
    return s_client_id;
}



/*===============================================================
 *                  START
 *==============================================================*/

/**
 * @brief Connect to the hub's own broker (call after local_broker_start)
 */
void local_client_start(void)
{
    char ap_ip[16];
    wifi_hub_ap_ip(ap_ip, sizeof(ap_ip));

    snprintf(s_uri, sizeof(s_uri), "mqtt://%s:%d", ap_ip, LOCAL_BROKER_PORT);
    snprintf(s_client_id, sizeof(s_client_id), "%s_bridge", hubIdentity.hub_id);

    esp_mqtt_client_config_t config = {
        .broker.address.uri             = s_uri,
        .credentials.client_id          = s_client_id,
        .network.disable_auto_reconnect = false,
        .session.keepalive              = 60,
        .buffer.size                    = 4096,
    };

    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to create local MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, local_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}