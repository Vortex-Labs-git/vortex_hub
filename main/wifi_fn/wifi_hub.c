/**
 * @file wifi_hub.c
 * @brief Hub Wi-Fi: own network for the devices + router connection + NAT
 *
 *   Valve / Sensor unit / Plug
 *            │  join "VHUB2026001" (SoftAP, 192.168.4.1)
 *            ▼
 *   ┌──────────────────┐   STA    ┌────────┐
 *   │    Vortex Hub    │ ───────► │ router │ ───► internet / server
 *   │  AP + STA + NAT  │          └────────┘
 *   └──────────────────┘
 *
 * This module:
 *  - Starts the hub's own Wi-Fi (always on, SSID = hub ID)
 *  - Connects to the router and retries forever (5 s, then up to 60 s)
 *  - When the router gives an IP:
 *      * devices get the router's DNS server from the hub's DHCP
 *      * the router side becomes the default route
 *      * NAT is switched on, so devices reach the internet (server, OTA, SNTP)
 *  - Counts the devices connected to the hub's Wi-Fi
 *
 * The AP follows the router's channel (one radio). When the router
 * changes channel, devices drop and re-join on their own.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "wifi_hub.h"
#include "global_fn/global_var.h"


/* ======================================================================== */
/* ========================== CONFIGURATION =============================== */
/* ======================================================================== */

#define AP_MAX_CLIENTS          CONFIG_HUB_AP_MAX_CLIENTS
#define HUB_TX_POWER            CONFIG_HUB_TX_POWER

#define RETRY_FIRST_S           5       // first router retry
#define RETRY_MAX_S             60      // retries slow down to this (less AP disturbance)

#define DHCPS_OFFER_DNS         0x02    // DHCP option: give clients a DNS server

static const char *TAG_AP  = "HUB_AP";
static const char *TAG_STA = "HUB_STA";

static esp_netif_t        *s_ap_netif   = NULL;
static esp_netif_t        *s_sta_netif  = NULL;
static esp_timer_handle_t  s_retry_timer = NULL;

static volatile bool s_router_connected = false;
static volatile int  s_client_count     = 0;
static int           s_retry_s          = RETRY_FIRST_S;



/* ======================================================================== */
/* ============================= NAT + DNS ================================ */
/* ======================================================================== */

/**
 * @brief Give devices the router's DNS server (they need it for the server, OTA, SNTP)
 */
static void ap_offer_router_dns(void)
{
    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK) {
        ESP_LOGW(TAG_AP, "Router gave no DNS server");
        return;
    }

    uint8_t offer = DHCPS_OFFER_DNS;

    esp_netif_dhcps_stop(s_ap_netif);
    esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer, sizeof(offer));
    esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns);
    esp_netif_dhcps_start(s_ap_netif);

    ESP_LOGI(TAG_AP, "Devices get DNS " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
}

/**
 * @brief Route devices to the internet through the router
 */
static void ap_enable_nat(void)
{
    esp_netif_set_default_netif(s_sta_netif);           // internet goes out via the router

    if (esp_netif_napt_enable(s_ap_netif) != ESP_OK) {
        ESP_LOGE(TAG_AP, "NAT could not be enabled (check CONFIG_LWIP_IPV4_NAPT)");
        return;
    }
    ESP_LOGI(TAG_AP, "NAT on: devices reach the internet through the hub");
}



/* ======================================================================== */
/* ========================== ROUTER RECONNECT ============================ */
/* ======================================================================== */

static void retry_timer_cb(void *arg)
{
    ESP_LOGI(TAG_STA, "Connecting to router \"%s\"...", hubWifi.router_ssid);
    esp_wifi_connect();
}

/**
 * @brief Try the router again after s_retry_s, then slow down (max 60 s)
 *
 * Every attempt scans channels, which briefly disturbs the AP, so a
 * missing router is retried less and less often.
 */
static void schedule_router_retry(void)
{
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)s_retry_s * 1000000ULL);

    ESP_LOGI(TAG_STA, "Router retry in %d s", s_retry_s);

    s_retry_s *= 2;
    if (s_retry_s > RETRY_MAX_S) {
        s_retry_s = RETRY_MAX_S;
    }
}



/* ======================================================================== */
/* ========================== WIFI EVENT HANDLER ========================== */
/* ======================================================================== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    /* ================= ROUTER (STA) EVENTS ================= */

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (strlen(hubWifi.router_ssid) > 0) {
            ESP_LOGI(TAG_STA, "Connecting to router \"%s\"...", hubWifi.router_ssid);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG_STA, "No router set: hub works offline (set it in the web UI)");
        }
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

        if (s_router_connected) {
            ESP_LOGW(TAG_STA, "Router lost (reason %d): devices stay on the hub, no internet", event->reason);
        } else {
            ESP_LOGW(TAG_STA, "Router not reachable (reason %d)", event->reason);
        }
        s_router_connected = false;

        if (strlen(hubWifi.router_ssid) > 0) {
            schedule_router_retry();
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Router connected, hub IP " IPSTR, IP2STR(&event->ip_info.ip));

        s_router_connected = true;
        s_retry_s = RETRY_FIRST_S;

        ap_offer_router_dns();
        ap_enable_nat();
    }

    /* ================= HUB WIFI (AP) EVENTS ================= */

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        s_client_count++;
        ESP_LOGI(TAG_AP, "Device joined " MACSTR " (%d connected)", MAC2STR(event->mac), s_client_count);
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        if (s_client_count > 0) s_client_count--;
        ESP_LOGI(TAG_AP, "Device left " MACSTR " (%d connected)", MAC2STR(event->mac), s_client_count);
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
        ESP_LOGI(TAG_AP, "Device " MACSTR " got IP " IPSTR, MAC2STR(event->mac), IP2STR(&event->ip));
    }
}



/* ======================================================================== */
/* ============================ CONFIGURATION ============================= */
/* ======================================================================== */

/**
 * @brief Hub's own Wi-Fi: SSID = hub ID, WPA2, password from NVS
 */
static void ap_configure(void)
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len       = strlen(hubIdentity.ap_ssid),
            .channel        = 1,                // follows the router once connected
            .max_connection = AP_MAX_CLIENTS,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg        = { .required = false },
        },
    };
    strlcpy((char *)ap_config.ap.ssid,     hubIdentity.ap_ssid, sizeof(ap_config.ap.ssid));
    strlcpy((char *)ap_config.ap.password, hubWifi.ap_pass,     sizeof(ap_config.ap.password));

    /* WPA2 needs 8+ characters; never fail the boot over it */
    if (strlen(hubWifi.ap_pass) < 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGE(TAG_AP, "Hub Wi-Fi password shorter than 8 characters: Wi-Fi is OPEN");
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_LOGI(TAG_AP, "Hub Wi-Fi \"%s\" (max %d devices)", hubIdentity.ap_ssid, AP_MAX_CLIENTS);
}

/**
 * @brief Router connection (skipped when no router is set)
 */
static void sta_configure(void)
{
    wifi_config_t sta_config = { 0 };

    strlcpy((char *)sta_config.sta.ssid,     hubWifi.router_ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, hubWifi.router_pass, sizeof(sta_config.sta.password));

    sta_config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.sae_pwe_h2e        = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
}



/* ======================================================================== */
/* ================================ START ================================= */
/* ======================================================================== */

/**
 * @brief Start the hub Wi-Fi (AP always on) and the router connection
 *
 * Call after NVS, esp_netif_init(), the default event loop, id_storage_load()
 * and wifi_storage_load().
 */
void wifi_hub_start(void)
{
    s_ap_netif  = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_event_handler, NULL, NULL));

    const esp_timer_create_args_t retry_args = {
        .callback = retry_timer_cb,
        .name     = "router_retry",
    };
    ESP_ERROR_CHECK(esp_timer_create(&retry_args, &s_retry_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));    // settings live in our own NVS blob
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ap_configure();
    sta_configure();

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(HUB_TX_POWER));
}



/* ======================================================================== */
/* ================================ STATUS ================================ */
/* ======================================================================== */

/**
 * @brief True while the hub has an IP from the router
 */
bool wifi_hub_router_connected(void)
{
    return s_router_connected;
}

/**
 * @brief Devices connected to the hub's Wi-Fi right now
 */
int wifi_hub_client_count(void)
{
    return s_client_count;
}

/**
 * @brief Hub's IP on the router network ("" when not connected)
 */
void wifi_hub_router_ip(char *out, size_t out_size)
{
    esp_netif_ip_info_t ip;

    out[0] = '\0';
    if (s_router_connected && esp_netif_get_ip_info(s_sta_netif, &ip) == ESP_OK) {
        snprintf(out, out_size, IPSTR, IP2STR(&ip.ip));
    }
}

/**
 * @brief Hub's IP on its own Wi-Fi (the devices' gateway, normally 192.168.4.1)
 */
void wifi_hub_ap_ip(char *out, size_t out_size)
{
    esp_netif_ip_info_t ip;

    out[0] = '\0';
    if (esp_netif_get_ip_info(s_ap_netif, &ip) == ESP_OK) {
        snprintf(out, out_size, IPSTR, IP2STR(&ip.ip));
    }
}