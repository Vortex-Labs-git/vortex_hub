/**
 * @file hub_time.c
 * @brief Test clock for the hub example
 *
 * This example has no WiFi, SNTP, GSM or RTC, so the clock starts at the
 * moment the firmware was BUILT (__DATE__ / __TIME__ of the build PC).
 * Good enough to test TIME_SYNC; the real hub uses SNTP / GSM / RTC.
 *
 * main/CMakeLists.txt touches this file on every build, so the
 * build time is always fresh.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"

#include "time_fn/hub_time.h"


static const char *TAG = "HUB_TIME";



/*===============================================================
 *                  INIT
 *==============================================================*/

/**
 * @brief Start the clock at the build time of the PC
 */
void hub_time_init(void)
{
    setenv("TZ", CONFIG_HUB_TIMEZONE, 1);
    tzset();

    /*----------------- Parse __DATE__ "Oct 24 2026" + __TIME__ "22:15:03" -----------------*/
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month_str[4] = { 0 };
    struct tm build = { 0 };

    sscanf(__DATE__, "%3s %d %d", month_str, &build.tm_mday, &build.tm_year);
    sscanf(__TIME__, "%d:%d:%d", &build.tm_hour, &build.tm_min, &build.tm_sec);

    build.tm_mon   = (int)((strstr(months, month_str) - months) / 3);
    build.tm_year -= 1900;
    build.tm_isdst = -1;

    /*----------------- Set the system clock -----------------*/
    struct timeval now = {
        .tv_sec  = mktime(&build),          // local build time -> UTC epoch
        .tv_usec = 0
    };
    settimeofday(&now, NULL);

    char text[32];
    hub_time_string(text, sizeof(text));
    ESP_LOGW(TAG, "Test clock started at build time: %s", text);
}



/*===============================================================
 *                  READ
 *==============================================================*/

/**
 * @brief Current time, UTC seconds since 1970 (what TIME_SYNC sends)
 */
int64_t hub_time_now(void)
{
    return (int64_t)time(NULL);
}

/**
 * @brief Current local time as text, e.g. "2026-10-24T22:15:03+0530"
 */
void hub_time_string(char *out, size_t out_size)
{
    time_t    now = time(NULL);
    struct tm local;

    localtime_r(&now, &local);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%S%z", &local);
}
