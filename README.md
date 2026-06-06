# ESP32-P4 720x720 Dashboard

Target hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-4B.

## Phase 1

- 720x720 LVGL UI structure
- Tabs for controls, sensors, and camera monitoring
- 3x4 control cards
- 3x4 sensor cards
- 2x2 camera cards
- Control metadata in `g_control_items[]`
- MQTT command publishing placeholder
- NVS storage placeholder

## Phase 2

This branch now uses the Waveshare BSP component as the display/touch/LVGL foundation:

- `waveshare/esp32_p4_wifi6_touch_lcd_4b^1.0.2`
- LCD/backlight initialization through `bsp_display_start_with_config()`
- GT911 touch through the BSP touch integration
- LVGL tick/task through `esp_lvgl_port`
- PSRAM + double-buffer display configuration for smoother 720x720 refresh
- MQTT command publishing and JSON state subscription
- HTTP/MJPEG camera card connection monitoring
- MQTT camera JPEG payload rendering from `jj/camera/image`
- MQTT sensor state updates from `jj/sensor/state`
- Editable per-control JSON settings dialog
- NVS persistence for the full control JSON array

## Device Config Designer

Open `tools/device_config_designer.html` in a browser to visually design a device config JSON file.

The designer supports:

- device ID, name, title, Wi-Fi, and MQTT fields
- visual editing for Controls, Sensors, and Cameras cards
- importing an existing `config/devices/*.json`
- copying or downloading the generated `<device_id>.json`

Place the exported file under `config/devices/<device_id>.json`, then run `tools/config_provider.py` to publish newer versions to the device over MQTT.

Do not commit real Wi-Fi passwords or private MQTT credentials. The included device JSON files keep those fields blank for sharing.

## Build

Use ESP-IDF 5.4.x and target `esp32p4`:

```powershell
idf.py set-target esp32p4
idf.py build
idf.py -p COM18 flash
```

## Control Commands

Control buttons call:

```text
https://emr.prof-jj.com/mqtt/publish_66.php?topic=JJ/iot/cmd&value={"device":"JJ/LED002","switch":"s1","cmd":"toggle"}
```

The endpoint is configured by `CONFIG_DASHBOARD_HTTP_PUBLISH_URL`.

## Expected Control State JSON

Control state updates are read from `CONFIG_DASHBOARD_MQTT_STATE_TOPIC`:

```json
{"id":"room_light","device":"JJ/LED001","switch":"s1","cmd":"on","state":true}
```

or:

```json
{"device":"JJ/LED001","switch":"s1","cmd":"off"}
```

## Camera Image JSON

Camera image payloads are read from `CONFIG_DASHBOARD_MQTT_CAMERA_TOPIC`, default `jj/camera/image`:

```json
{
  "device_id": "RIEtEXly",
  "mime": "image/jpeg",
  "width": 320,
  "height": 240,
  "ip": "192.168.1.26",
  "ts": 338590544,
  "motion_score": 79.31,
  "rotation": 0,
  "image_base64": "/9j/..."
}
```

`RIEtEXly` is mapped to the first camera card by default.

## Sensor State JSON

Sensor updates are read from `CONFIG_DASHBOARD_MQTT_SENSOR_TOPIC`, default `jj/sensor/state`:

```json
{
  "device_id": "XffcKwL9",
  "temperature": 24.8,
  "humidity": 56.8
}
```

`XffcKwL9` updates the first two sensor cards by default.

## Next Steps

1. Configure `DASHBOARD_MQTT_BROKER_URI`, MQTT topics, and the HTTP publish URL in menuconfig.
2. Replace the default camera URLs in `main/camera/camera_stream.c`.
3. Expand the settings dialog from raw JSON to form controls if touch editing feels too slow.
