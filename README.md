# ESP32-P4 720x720 Dashboard Prototype

目標硬體：Waveshare ESP32-P4-WIFI6-Touch-LCD-4B。

第一版包含：
- 720x720 UI 架構
- 三個 Tab：控制、感測器、影像監控
- 控制頁 3x4 卡片
- 感測器頁 3x4 卡片
- 影像監控頁 2x2 大卡片
- 每個控制元件有獨立參數表 `g_control_items[]`
- 長按控制卡片開啟設定頁占位
- MQTT 發送函式占位
- NVS 儲存函式占位

## 下一步
1. 下載 Waveshare 官方 ESP-IDF 範例。
2. 把本專案的 `main/ui`、`main/app_config`、`main/mqtt`、`main/storage` 合併進官方 LCD/touch 範例。
3. 在 `app_main.c` 依官方 BSP 初始化 LCD、touch、LVGL tick。
4. 接上 Wi-Fi 與 MQTT。
5. 完成 `ui_settings.c` 表單編輯與 NVS 儲存。

## 注意
此骨架專注 UI 與資料架構。LCD/MIPI-DSI/Touch 初始化需沿用 Waveshare 官方範例，避免不同批次板子驅動參數不同。
