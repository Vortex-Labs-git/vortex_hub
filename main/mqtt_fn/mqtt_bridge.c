/**
 * @file mqtt_bridge.c
 * @brief Moves messages between the devices (local broker) and the server
 *
 *   UP    device -> local broker -> [bridge] -> device variables + server
 *         status, state_data, error
 *
 *   DOWN  server -> cloud client -> [bridge] -> device variables + local broker (retained)
 *         cmd_data, control_data
 *
 * Each sub-topic goes one way only, so nothing loops back. The JSON is
 * forwarded byte for byte: the server and the device see exactly what
 * the other side sent. A copy is parsed into the device's variables.
 *
 * Only devices in the hub's device list pass; anything else is dropped.
 *
 * The broker and the MQTT client call in from their own tasks; they only
 * copy the message into a queue. The bridge task does the work.
 */

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "mqtt_fn/mqtt_bridge.h"
#include "mqtt_fn/cloud_client.h"
#include "mqtt_fn/local_client.h"
#include "device_fn/vortex_dev.h"


#define BRIDGE_QUEUE_LEN    32
#define BRIDGE_TASK_STACK   6144
#define BRIDGE_TASK_PRIO    5
#define TIMEOUT_CHECK_MS    1000

#define DEVICE_TIMEOUT_S    CONFIG_HUB_DEVICE_TIMEOUT_S

static const char *TAG = "MQTT_BRIDGE";

typedef enum {
    FROM_DEVICE,
    FROM_SERVER,
} msg_source_t;

typedef struct {
    msg_source_t source;
    char        *topic;         // heap (PSRAM), freed by the bridge task
    char        *data;
    int          len;
} bridge_msg_t;

static QueueHandle_t s_queue = NULL;



/*===============================================================
 *                  DIRECTION RULES
 *==============================================================*/

static bool is_up_topic(const char *sub)
{
    return strcmp(sub, "status") == 0 || strcmp(sub, "state_data") == 0 || strcmp(sub, "error") == 0;
}

static bool is_down_topic(const char *sub)
{
    return strcmp(sub, "cmd_data") == 0 || strcmp(sub, "control_data") == 0;
}



/*===============================================================
 *                  QUEUE (called from other tasks)
 *==============================================================*/

static char *copy_text(const char *src, int len)
{
    char *dst = heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM);
    if (dst == NULL) {
        dst = malloc(len + 1);
    }
    if (dst != NULL) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}

static void enqueue(msg_source_t source, const char *topic, const char *data, int len)
{
    if (s_queue == NULL || topic == NULL || data == NULL || len < 0) {
        return;
    }

    bridge_msg_t msg = {
        .source = source,
        .topic  = copy_text(topic, strlen(topic)),
        .data   = copy_text(data, len),
        .len    = len,
    };

    if (msg.topic == NULL || msg.data == NULL || xQueueSend(s_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Bridge queue full or no memory, dropped %s", topic);
        free(msg.topic);
        free(msg.data);
    }
}

/**
 * @brief A device published on the local broker (broker task)
 */
void mqtt_bridge_from_device(const char *topic, const char *data, int len)
{
    enqueue(FROM_DEVICE, topic, data, len);
}

/**
 * @brief The server sent a message for one of our devices (cloud client task)
 */
void mqtt_bridge_from_server(const char *topic, const char *data, int len)
{
    enqueue(FROM_SERVER, topic, data, len);
}



/*===============================================================
 *                  ROUTING (bridge task)
 *==============================================================*/

/**
 * @brief UP: device -> its variables -> server
 */
static void route_up(const bridge_msg_t *msg)
{
    char sub[24];
    Vortex_Dev *dev = vortex_dev_from_topic(msg->topic, sub, sizeof(sub));

    if (dev == NULL || !is_up_topic(sub)) {
        return;                                 // not ours, or our own down message
    }

    vortex_dev_save_device_msg(dev, sub, msg->data, msg->len);
    cloud_client_publish(msg->topic, msg->data, msg->len, false);
}

/**
 * @brief DOWN: server -> its variables -> device (retained, so a device that
 *        reconnects gets its settings at once)
 */
static void route_down(const bridge_msg_t *msg)
{
    char sub[24];
    Vortex_Dev *dev = vortex_dev_from_topic(msg->topic, sub, sizeof(sub));

    if (dev == NULL || !is_down_topic(sub)) {
        return;
    }

    vortex_dev_save_server_msg(dev, sub, msg->data, msg->len);
    local_client_publish(msg->topic, msg->data, msg->len, true);
}

static void mqtt_bridge_task(void *pvParameters)
{
    bridge_msg_t msg;

    while (1) {
        if (xQueueReceive(s_queue, &msg, pdMS_TO_TICKS(TIMEOUT_CHECK_MS)) == pdTRUE) {

            if (msg.source == FROM_DEVICE) {
                route_up(&msg);
            } else {
                route_down(&msg);
            }

            free(msg.topic);
            free(msg.data);
        }

        vortex_dev_check_timeouts(DEVICE_TIMEOUT_S);
    }
}



/*===============================================================
 *                  START
 *==============================================================*/

/**
 * @brief Create the queue and the bridge task (before the broker and clients)
 */
void mqtt_bridge_start(void)
{
    s_queue = xQueueCreate(BRIDGE_QUEUE_LEN, sizeof(bridge_msg_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create bridge queue");
        return;
    }

    if (xTaskCreate(mqtt_bridge_task, "mqtt_bridge", BRIDGE_TASK_STACK, NULL, BRIDGE_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bridge task");
    }
}