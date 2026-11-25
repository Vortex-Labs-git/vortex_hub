/**
 * @file global_var.c
 * @brief Global shared data structures for system-wide state management
 *
 * This file defines the global structures used across:
 *  - Wi-Fi (hub AP + router STA)
 *  - Web UI (later)
 *  - MQTT bridge (later)
 */

#include "global_fn/global_var.h"



/* ======================================================================== */
/* ============================ HUB IDENTITY ============================== */
/* ======================================================================== */

/**
 * @brief Hub ID and Wi-Fi name (loaded from NVS by id_storage_load)
 */
HubIdentity hubIdentity = {
    .hub_id  = "",
    .ap_ssid = ""
};



/* ======================================================================== */
/* ============================== HUB WIFI ================================ */
/* ======================================================================== */

/**
 * @brief Router credentials + hub Wi-Fi password (loaded by wifi_storage_load)
 */
HubWifi hubWifi = {
    .router_ssid = "",
    .router_pass = "",
    .ap_pass     = "",
    .set_wifi    = false
};