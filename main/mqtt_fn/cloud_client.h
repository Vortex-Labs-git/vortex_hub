#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <stdbool.h>

void cloud_client_start(void);
bool cloud_client_connected(void);

bool cloud_client_publish(const char *topic, const char *data, int len, bool retain);

#endif // CLOUD_CLIENT_H