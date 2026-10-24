#ifndef HUB_TIME_H
#define HUB_TIME_H

#include <stddef.h>
#include <stdint.h>

void    hub_time_init(void);

int64_t hub_time_now(void);
void    hub_time_string(char *out, size_t out_size);

#endif // HUB_TIME_H
