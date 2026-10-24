# Vortex Hub — ESP-NOW link test example

ESP32-S3 example that tests the ESP-NOW link between the Vortex Hub and the
smart valve (`vortex_valve_esp`, branch `v4.1`). No WiFi router, no MQTT:
every message the real hub would publish is printed as **MQTT topic + raw JSON**.

## What it does

| Direction | Message | Hub action |
|---|---|---|
| valve → hub | `PROBE` | answered by the radio (channel found) |
| valve → hub | `CALLX` | checks the pair key, adds the device, sends `CALLX_ACK` + `TIME_SYNC` |
| valve → hub | `status` / `state_data` / `error` | prints `vortex_device/<type>/<id>/<topic>` + JSON |
| valve → hub | data from an unknown device | sends `REJOIN` (device pairs again) |
| hub → valve | `TIME_SYNC` | after every pairing and every `HUB_TIME_SYNC_PERIOD_S` (1 h) |
| hub → valve | `cmd_data` | BOOT button: valve angle 90 / 0, sent with end-to-end ACK |

Every minute it prints the device table and the link counters.

## Project layout

```
main/
├── main.c                  start-up: NVS, test clock, WiFi STA on fixed channel, ESP-NOW
├── espnow_fn/              SHARED with the devices — keep byte-for-byte identical
├── hub_fn/
│   ├── device_table.c/.h   paired devices (RAM only in this example)
│   ├── hub_link.c/.h       CALLX, CALLX_ACK, TIME_SYNC, REJOIN, data print
│   └── hub_test.c/.h       BOOT button -> test cmd_data
└── time_fn/
    └── hub_time.c/.h       test clock = build time of the PC
```

## Build and flash (ESP-IDF 5.4.x, same as the valve)

```
idf.py set-target esp32s3
idf.py menuconfig          # Vortex Hub Example (defaults are fine)
idf.py build flash monitor
```

The banner at boot prints the two values the valve needs:

```
 Valve menuconfig -> Vortex Hub Link -> HUB Configuration:
   HUB_DEFAULT_MAC      = xx:xx:xx:xx:xx:xx
   HUB_DEFAULT_PAIR_KEY = 00112233445566778899aabbccddeeff
```

## Valve setup

1. `idf.py menuconfig` → **Vortex Hub Link → HUB Configuration**
   - `HUB_DEFAULT_MAC` = MAC from the hub banner
   - `HUB_DEFAULT_PAIR_KEY` = same key as the hub
   - `HUB_STORAGE_RESET` = **y** for this first flash (overwrites any saved hub)
2. `idf.py build flash monitor`
3. After it pairs once, set `HUB_STORAGE_RESET` back to **n**.

## Test plan

| # | Test | Expected |
|---|---|---|
| 1 | Power hub, then valve | valve: `State SEARCH -> LINKED`, `Time synced from hub`; hub: `NEW wifi_valve ...` |
| 2 | Wait 5 s | hub prints `vortex_device/wifi_valve/<id>/state_data`, `status`, `error` with JSON |
| 3 | Press BOOT on the hub | hub: `DELIVERED (ACK)`; valve moves to 90°, next press 0° |
| 4 | Reset the hub | hub: `Data from unknown ... -> REJOIN`; valve pairs again within a few seconds |
| 5 | Power off the hub | valve: `Link to hub lost`, SEARCH 45 s, AP window 90 s, repeat; hub back on → LINKED |
| 6 | Wrong pair key on the valve | hub: `CALLX ... REFUSED: wrong pair key`; valve: `No valid CALLX_ACK` |
| 7 | Change `HUB_CHANNEL` on the hub | valve finds the new channel by itself |

## Limits of this example

- Test clock starts at the build time of the PC (no SNTP / GSM / RTC).
- Device table is in RAM: a hub reset forgets devices (used to test REJOIN).
- No encryption on the ESP-NOW frames yet (CALLX is HMAC-signed).
