#!/usr/bin/env bash
# Layering check (restructure.md 5.2): lib/core must stay free of Arduino, LVGL, FreeRTOS and ESP-IDF headers.
set -euo pipefail

cd "$(dirname "$0")/.."

CORE_DIR="lib/core"
FORBIDDEN='#[[:space:]]*include[[:space:]]*[<"](Arduino\.h|lvgl[./]|lv_conf\.h|freertos/|FreeRTOS|esp_|esp32|driver/|soc/|nvs|Preferences\.h|LovyanGFX|WiFi|SPI\.h|Wire\.h)'

if [ ! -d "$CORE_DIR" ]; then
    echo "check_layering: $CORE_DIR does not exist" >&2
    exit 1
fi

if matches=$(grep -rnE "$FORBIDDEN" "$CORE_DIR" --include='*.h' --include='*.hpp' --include='*.cpp' --include='*.c'); then
    echo "check_layering: forbidden platform includes in $CORE_DIR:" >&2
    echo "$matches" >&2
    exit 1
fi

echo "check_layering: OK"
