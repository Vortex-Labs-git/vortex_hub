#ifndef WIFI_HUB_H
#define WIFI_HUB_H

#include <stdbool.h>
#include <stddef.h>


void wifi_hub_start(void);

bool wifi_hub_router_connected(void);
int  wifi_hub_client_count(void);
void wifi_hub_router_ip(char *out, size_t out_size);

void wifi_hub_ap_ip(char *out, size_t out_size);

#endif // WIFI_HUB_H