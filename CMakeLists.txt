# Vortex Hub (ESP32-S3) — devices join the hub's Wi-Fi, the hub routes them to the server
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vortex_hub)