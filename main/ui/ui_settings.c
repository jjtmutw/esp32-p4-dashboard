#include "lvgl.h"
#include "ui_settings.h"
#include "storage/nvs_config.h"

void ui_settings_open(control_item_t *item) {
    lv_obj_t *box = lv_msgbox_create(NULL);
    lv_msgbox_add_title(box, "控制元件設定");
    lv_msgbox_add_text(box, "長按卡片開啟。下一版會加入完整表單：名稱、Topic、Device、Switch、ON/OFF 指令、自動關閉、確認開關。");
    lv_msgbox_add_close_button(box);
    LV_UNUSED(item);
}
