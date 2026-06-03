#include "nvs_config.h"
#include "nvs_flash.h"

void nvs_config_init(void) {
    nvs_flash_init();
}
void nvs_config_load_controls(void) {
    // TODO: load g_control_items from NVS blob.
}
void nvs_config_save_controls(void) {
    // TODO: save g_control_items to NVS blob.
}
