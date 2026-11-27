#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

void mqtt_bridge_start(void);

/* Called from the broker / server client: copy the message into the bridge queue */
void mqtt_bridge_from_device(const char *topic, const char *data, int len);
void mqtt_bridge_from_server(const char *topic, const char *data, int len);

#endif // MQTT_BRIDGE_H