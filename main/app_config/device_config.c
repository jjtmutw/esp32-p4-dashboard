#include "device_config.h"

control_item_t g_control_items[CONTROL_ITEM_COUNT] = {
    {"room_light", "溫室燈", "照明控制", "light", "JJ/iot/cmd", "JJ/LED001", "s1", "on", "off", false, 0, true, true},
    {"sprinkler", "溫室灑水器", "灌溉系統", "water", "JJ/iot/cmd", "JJ/LED001", "s2", "on", "off", false, 0, true, true},
    {"fan", "溫室風扇", "通風設備", "fan", "JJ/iot/cmd", "JJ/LED001", "s3", "on", "off", false, 0, true, true},
    {"eco_power", "生態缸電源", "電源控制", "power", "JJ/iot/cmd", "JJ/LED001", "s4", "on", "off", true, 0, true, true},
    {"night_light", "一號小夜燈", "走廊照明", "lamp", "JJ/iot/cmd", "JJ/LED002", "s1", "on", "off", false, 0, true, true},
    {"windmill", "小風車的燈", "裝飾照明", "lamp", "JJ/iot/cmd", "JJ/LED002", "s2", "on", "off", false, 0, true, true},
    {"machine_power", "機房電源", "設備供電", "plug", "JJ/iot/cmd", "JJ/LED002", "s3", "on", "off", true, 0, true, false},
    {"alert_old", "注意老闆來了", "警示提醒", "bell", "JJ/alert/cmd", "JJ/ALERT001", "s1", "send", "off", false, 0, true, false},
    {"voice_remind", "語音提醒開關", "語音播報", "speaker", "JJ/tts/cmd", "JJ/TTS001", "s1", "on", "off", false, 0, true, true},
    {"office_cam", "辦公室相機", "攝影機電源", "camera", "JJ/cam/cmd", "CAM001", "power", "on", "off", false, 0, true, false},
    {"plant_light", "植物生長燈", "植物照明", "plant", "JJ/iot/cmd", "JJ/LED003", "s1", "on", "off", false, 0, true, true},
    {"reboot_all", "所有設備重啟", "系統重啟", "reboot", "JJ/system/cmd", "all", "reboot", "reboot", "none", true, 0, true, false},
};
