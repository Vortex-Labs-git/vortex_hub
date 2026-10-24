/**
 * @file hub_test.c
 * @brief Downlink test: BOOT button -> cmd_data to every online valve
 *
 * Each press alternates the valve angle 90 / 0 and sends the same
 * "set_valve_basic" JSON the server sends today on .../cmd_data.
 * Sent with ESPNOW_FLAG_ACK_REQ, so the log shows whether the valve
 * really received it.
 */

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"

#include "hub_fn/hub_test.h"
#include "hub_fn/device_table.h"
#include "time_fn/hub_time.h"
#include "espnow_fn/espnow_send.h"


#define TEST_BUTTON_GPIO    CONFIG_HUB_TEST_BUTTON_GPIO
#define POLL_MS             50

static const char *TAG = "HUB_TEST";



/*===============================================================
 *                  BUILD TEST COMMAND
 *==============================================================*/

/**
 * @brief Same JSON as the server's cmd_data (valve: mqtt_handle_cmd_data)
 */
static char *build_cmd_data(const char *device_id, int angle)
{
    char timestamp[32];
    hub_time_string(timestamp, sizeof(timestamp));

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "event", "set_valve_basic");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", device_id);

    cJSON *controller = cJSON_AddObjectToObject(json, "set_controller");
    cJSON_AddBoolToObject(controller, "schedule", false);
    cJSON_AddBoolToObject(controller, "sensor", false);

    cJSON *valve = cJSON_AddObjectToObject(json, "valve_data");
    cJSON_AddStringToObject(valve, "name", "hub-test");
    cJSON_AddBoolToObject(valve, "set_angle", true);
    cJSON_AddNumberToObject(valve, "angle", angle);

    char *text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return text;
}



/*===============================================================
 *                  SEND TO ALL ONLINE VALVES
 *==============================================================*/

static void send_test_command(int angle)
{
    hub_device_t online[CONFIG_HUB_MAX_DEVICES];
    int count = device_get_online(online, CONFIG_HUB_MAX_DEVICES);
    int sent  = 0;

    for (int i = 0; i < count; i++) {
        if (online[i].dev_type != ESPNOW_DEV_VALVE) {
            continue;
        }

        char *json = build_cmd_data(online[i].device_id, angle);
        if (json == NULL) {
            continue;
        }

        esp_err_t err = espnow_send_msg(online[i].mac, ESPNOW_MSG_CMD_DATA, json, strlen(json), ESPNOW_FLAG_ACK_REQ);

        ESP_LOGI(TAG, "cmd_data angle %d -> %s : %s", angle, online[i].device_id,
                 err == ESP_OK ? "DELIVERED (ACK)" : esp_err_to_name(err));
        free(json);
        sent++;
    }

    if (sent == 0) {
        ESP_LOGW(TAG, "No online valve to send to");
    }
}



/*===============================================================
 *                  BUTTON TASK
 *==============================================================*/

/**
 * @brief Poll the button; one command per press (on release of a press)
 */
static void hub_test_task(void *pvParameters)
{
    bool was_pressed = false;
    int  angle       = 90;

    while (1) {
        bool pressed = (gpio_get_level(TEST_BUTTON_GPIO) == 0);     // active low

        if (pressed && !was_pressed) {
            ESP_LOGI(TAG, "Test button pressed");
            send_test_command(angle);
            angle = (angle == 90) ? 0 : 90;
        }

        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

/**
 * @brief Configure the button and start polling it
 */
void hub_test_start(void)
{
    gpio_config_t button = {
        .pin_bit_mask = 1ULL << TEST_BUTTON_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&button);

    xTaskCreate(hub_test_task, "hub_test", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Press the button on GPIO %d to send cmd_data to all valves", TEST_BUTTON_GPIO);
}
