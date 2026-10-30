# Sources and licenses

- Original MOCHI application, vector character, animation, tests and preview tooling: Apache-2.0. No third-party pet project code or character artwork is incorporated.
- `scripts/device.py` is reused without modification from the local RoundNav project's Apache-2.0 backup-and-flash helper. It does not include navigation or Bluetooth application code.
- Board support: `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0, Apache-2.0. https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C
- ESP-IDF 5.5.5: Apache-2.0 and per-component notices. https://github.com/espressif/esp-idf/tree/v5.5.5
- LVGL 9.5.0: MIT. https://github.com/lvgl/lvgl
- `main/pet_font.c` bitmap glyphs and the bundled source font subset derive from Noto Sans SC under SIL Open Font License 1.1, renamed MOCHI UI 18 for this application. See `assets/fonts/OFL.txt` and `assets/fonts/font-source.json`. https://fonts.google.com/noto/specimen/Noto+Sans+SC
- Other managed components retain their own licenses; exact versions are recorded in `dependencies.lock`. Firmware distributions include the managed components' root license/notice files under `third_party/`.
