#ifndef LOCAL_CLIENT_H
#define LOCAL_CLIENT_H

#include <stdbool.h>

void        local_client_start(void);
bool        local_client_connected(void);
const char *local_client_id(void);

bool        local_client_publish(const char *topic, const char *data, int len, bool retain);

#endif // LOCAL_CLIENT_H