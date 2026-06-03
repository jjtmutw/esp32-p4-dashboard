#pragma once
#include <stdbool.h>

#define CONTROL_ITEM_COUNT 12

typedef struct {
    char id[32];
    char name[32];
    char subtitle[32];
    char icon[16];
    char mqtt_topic[64];
    char device[32];
    char sw[8];
    char cmd_on[16];
    char cmd_off[16];
    bool confirm;
    int auto_off_sec;
    bool visible;
    bool state;
} control_item_t;

extern control_item_t g_control_items[CONTROL_ITEM_COUNT];
