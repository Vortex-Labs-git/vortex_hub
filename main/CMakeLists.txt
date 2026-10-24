idf_component_register(SRCS
                            "main.c"
                            "espnow_fn/espnow_util.c"
                            "espnow_fn/espnow_pair.c"
                            "espnow_fn/espnow_send.c"
                            "espnow_fn/espnow_recv.c"
                            "espnow_fn/espnow_link.c"
                            "hub_fn/device_table.c"
                            "hub_fn/hub_link.c"
                            "hub_fn/hub_test.c"
                            "time_fn/hub_time.c"
                        INCLUDE_DIRS
                            "."
                        REQUIRES
                            esp_wifi
                            esp_netif
                            esp_event
                            nvs_flash
                            json
                            driver
                            mbedtls
                            esp_timer
                            )

# Hub rebuilds messages from many devices at once (devices use 4)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESPNOW_RECV_SLOTS=20)

# Test clock = build time: recompile hub_time.c on every build so __DATE__/__TIME__ stay fresh
add_custom_target(touch_hub_time ALL COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/time_fn/hub_time.c)
add_dependencies(${COMPONENT_LIB} touch_hub_time)
