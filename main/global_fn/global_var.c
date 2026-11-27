/**
 * @file global_var.c
 * @brief Global shared data structures for system-wide state management
 *
 * This file defines the global structures used across:
 *  - Wi-Fi (hub AP + router STA)
 *  - Device list (what the user expects to connect)
 *  - Device objects (local DB: one per expected device)
 */

#include <stddef.h>

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



/* ======================================================================== */
/* ============================ DEVICE LIST =============================== */
/* ======================================================================== */

/**
 * @brief Devices expected on this hub (loaded by device_storage_load)
 */
HubDeviceList hubDeviceList = {
    .count = 0
};



/* ======================================================================== */
/* =========================== DEVICE OBJECTS ============================= */
/* ======================================================================== */

/**
 * @brief Local DB: created at boot by vortex_dev_init_all(), one per device
 */
Vortex_VA *vortexValves      = NULL;
int        vortexValveCount  = 0;

Vortex_SU *vortexSensors     = NULL;
int        vortexSensorCount = 0;

Vortex_SP *vortexPlugs       = NULL;
int        vortexPlugCount   = 0;