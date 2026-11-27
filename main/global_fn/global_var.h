#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


#define HUB_MAX_DEVICES         15      // = max devices on the hub Wi-Fi
#define DEVICE_ID_LEN           24      // "VA202611301"
#define TOPIC_BASE_LEN          64      // "vortex_device/wifi_valve/VA202611301"



/* ======================================================================== */
/* ================================= HUB ================================== */
/* ======================================================================== */

// Define the structure for hub identity
typedef struct {
    char hub_id[24];            // "VHUB202611001"
    char ap_ssid[32];           // hub Wi-Fi name "Vortex_VHUB202611001" (devices look for "Vortex_VHUB")
} HubIdentity;

// Define the structure for hub Wi-Fi settings
typedef struct {
    char router_ssid[32];       // router the hub connects to (internet)
    char router_pass[64];
    char ap_pass[64];           // password of the hub's own Wi-Fi
    bool set_wifi;              // true = set by the user, false = menuconfig default
} HubWifi;



/* ======================================================================== */
/* ======================= DEVICES EXPECTED (NVS) ========================= */
/* ======================================================================== */

typedef enum {
    DEV_VALVE = 0,              // topic "wifi_valve"
    DEV_SENSOR,                 // topic "sensor_unit"
    DEV_PLUG,                   // topic "smart_plug"
    DEV_TYPE_COUNT
} DevType;

// One device the user added in the hub config
typedef struct {
    uint8_t type;               // DevType
    char    device_id[DEVICE_ID_LEN];
} HubDeviceEntry;

// The device list saved in NVS (hub restarts after it changes)
typedef struct {
    uint8_t        count;
    HubDeviceEntry devices[HUB_MAX_DEVICES];
} HubDeviceList;



/* ======================================================================== */
/* ====================== COMMON PART OF EVERY DEVICE ===================== */
/* ======================================================================== */

typedef struct {
    DevType           type;
    char              device_id[DEVICE_ID_LEN];
    char              topic_base[TOPIC_BASE_LEN];   // built at boot from type + id
    bool              online;
    int64_t           last_seen_ms;                 // hub uptime of the last message
    char              bin_version[16];              // from status
    SemaphoreHandle_t mutex;                        // guards the whole device object
} Vortex_Dev;



/* ======================================================================== */
/* ================================ VALVE ================================= */
/* ======================================================================== */

// From the valve (status / state_data / error)
typedef struct {
    char  timestamp[32];
    bool  schedule_control;
    bool  sensor_control;
    int   angle;
    bool  is_open;
    bool  is_close;
    bool  open_limit_available;
    bool  open_limit_click;
    bool  close_limit_available;
    bool  close_limit_click;
    char  error_msg[100];
} VA_DeviceData;

// From the server (cmd_data / control_data)
typedef struct {
    bool  schedule_control;
    bool  sensor_control;
    char  name[32];
    bool  set_angle;
    int   angle;
    char  ota_version[16];
    int   schedule_count;       // entries in control_data.schedule.schedule_info
    char  sensor_unit_id[DEVICE_ID_LEN];    // linked sensor (step 2)
    char  sensor_id[8];
    int   sensor_rule_count;
} VA_ServerData;

typedef struct {
    Vortex_Dev    dev;
    VA_DeviceData device;
    VA_ServerData server;
} Vortex_VA;



/* ======================================================================== */
/* ============================= SENSOR UNIT ============================== */
/* ======================================================================== */

#define SU_MAX_SENSORS          8

typedef struct {
    char  sensor_id[8];         // "S00", "S01", ...
    char  sensor_type[24];      // "Temperature"
    char  sensor_name[32];
    float sensor_value;
    char  error[64];
} SU_Sensor;

// From the sensor unit (status / state_data / error)
typedef struct {
    char      timestamp[32];
    int       sensor_count;
    SU_Sensor sensors[SU_MAX_SENSORS];
} SU_DeviceData;

// From the server (cmd_data: only OTA today)
typedef struct {
    char  ota_version[16];
} SU_ServerData;

typedef struct {
    Vortex_Dev    dev;
    SU_DeviceData device;
    SU_ServerData server;
} Vortex_SU;



/* ======================================================================== */
/* ================================ PLUG ================================== */
/* ======================================================================== */

#define SP_BASES                2       // base_A, base_B

// From the plug, per socket
typedef struct {
    bool  schedule_control;
    bool  sensor_control;
    char  name[32];
    bool  state;                // reported ON / OFF
    int   current;
} SP_BaseState;

typedef struct {
    char         timestamp[32];
    SP_BaseState base[SP_BASES];
    char         error_msg[100];
} SP_DeviceData;

// From the server, per socket
typedef struct {
    bool  schedule_control;
    bool  sensor_control;
    char  name[32];
    bool  state;                // requested ON / OFF
    int   schedule_count;
    char  sensor_unit_id[DEVICE_ID_LEN];
    char  sensor_id[8];
    int   sensor_rule_count;
} SP_BaseCommand;

typedef struct {
    SP_BaseCommand base[SP_BASES];
    char           ota_version[16];
} SP_ServerData;

typedef struct {
    Vortex_Dev    dev;
    SP_DeviceData device;
    SP_ServerData server;
} Vortex_SP;



/* ======================================================================== */
/* ======================== DECLARE GLOBAL VARIABLES ====================== */
/* ======================================================================== */

extern HubIdentity   hubIdentity;
extern HubWifi       hubWifi;
extern HubDeviceList hubDeviceList;

// Created at boot from hubDeviceList: one entry per expected device
extern Vortex_VA *vortexValves;
extern int        vortexValveCount;
extern Vortex_SU *vortexSensors;
extern int        vortexSensorCount;
extern Vortex_SP *vortexPlugs;
extern int        vortexPlugCount;

#endif // GLOBAL_VAR_H