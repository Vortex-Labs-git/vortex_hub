#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>
#include <stdint.h>


// Define the structure for hub identity
typedef struct {
    char hub_id[32];            // "VHUB2026001"
    char ap_ssid[32];           // hub Wi-Fi name = hub_id (devices look for "VHUB")
} HubIdentity;


// Define the structure for hub Wi-Fi settings
typedef struct {
    char router_ssid[32];       // router the hub connects to (internet)
    char router_pass[64];
    char ap_pass[64];           // password of the hub's own Wi-Fi
    bool set_wifi;              // true = set by the user, false = menuconfig default
} HubWifi;


// Declare the global variables
extern HubIdentity hubIdentity;
extern HubWifi     hubWifi;

#endif // GLOBAL_VAR_H