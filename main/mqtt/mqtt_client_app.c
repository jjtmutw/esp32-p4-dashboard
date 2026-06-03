#include <stdio.h>
#include "mqtt_client_app.h"

void mqtt_app_start(void) {
    // TODO: connect Wi-Fi first, then start esp-mqtt client.
}

void mqtt_publish_control(control_item_t *item, bool on) {
    const char *cmd = on ? item->cmd_on : item->cmd_off;
    printf("MQTT topic=%s payload={\"device\":\"%s\",\"switch\":\"%s\",\"cmd\":\"%s\"}\n",
           item->mqtt_topic, item->device, item->sw, cmd);
}
