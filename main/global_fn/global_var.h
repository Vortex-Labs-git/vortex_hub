#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"



#define MAX_VA                  CONFIG_HUB_MAX_VALVES
#define VA_MAX_SCHEDULES        10
#define VA_MAX_SENSOR_RULES     10


/*---------------------------------------------------------------
 * Common device info
 *--------------------------------------------------------------*/
typedef struct {
    uint8_t  mac[6];
    char     device_id[32];
    char     fw_version[16];        // from CALLX
    bool     online;
    int64_t  last_seen_ms;
    int      rssi;
} Vortex_Dev;


// --------------------------------------------------------------
//   Valve 
// --------------------------------------------------------------

// Data from VA device -------------------
typedef struct {
    char  timestamp[32]; 

    char  status[16];
    char  bin_version[16];
} VA_Status;

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
} VA_StateData;

typedef struct {
    char  timestamp[32]; 

    char  error_msg[100]; 
} VA_Error;

// Data from Server ----------------------
typedef struct {
    char  day[16];
    char  open[12];
    char  close[12];
    int   angle;
} VA_ScheduleInfo;

typedef struct {
    int   low;
    int   high;
    int   angle;
} VA_SensorRule;

typedef struct {
    char  unit_id[16];
    char  sensor_id[16];
    char  sensor_name[32];
    char  last_seen[32];
    char  sensor_type[24];
    float sensor_value;
} VA_SensorData;

typedef struct {
    char    timestamp[32]; 

    bool    schedule_control;
    bool    sensor_control;

    char    name[32];
    bool    set_angle;
    int     angle;

    char    ota_version[16];
    char    ota_url[128];
} VA_CmdData;

typedef struct {
    char    timestamp[32]; 

    VA_ScheduleInfo   schedule_info[10];
    int           schedule_count;

    VA_SensorRule sensor_rules[10];
    int           sensor_rule_count;
    
    VA_SensorData sensor_data;
} VA_ControlData;

// Valve Object ---------------------------
typedef struct {
    bool                used;
    Vortex_Dev          device;

    // Device side
    VA_Status           status;
    bool                status_new;
    VA_StateData        state_data;
    bool                state_new;
    VA_Error            error;
    bool                error_new;
    SemaphoreHandle_t   device_mutex;

    // Server side
    VA_CmdData          cmd_data;
    bool                cmd_pending;
    VA_ControlData      control_data;
    bool                control_pending;
    SemaphoreHandle_t   server_mutex;
} Vortex_VA;






extern Vortex_VA vortexValves[MAX_VA];



#endif // GLOBAL_VAR_H