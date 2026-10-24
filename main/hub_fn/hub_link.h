#ifndef HUB_LINK_H
#define HUB_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "espnow_fn/espnow_proto.h"


void hub_link_start(void);

void hub_link_on_message(const uint8_t mac[6], const espnow_hdr_t *hdr, const uint8_t *data, size_t len, int rssi);

#endif // HUB_LINK_H
