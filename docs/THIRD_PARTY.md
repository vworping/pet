# Third-party sources

- `firmware/main/board/lvgl_port.[ch]`: Waveshare ESP32-S3-Touch-LCD-5 official
  `ESP-IDF/08_lvgl_Porting` example; Espressif Apache-2.0 notices retained.
- `firmware/main/board/waveshare_rgb_lcd_port.[ch]`: same example, CC0-1.0
  notice retained. Demo chart code removed; initialization errors checked.
- `firmware/main/Kconfig.projbuild`: display settings from the same example.
- Source archive: https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-5/ESP32-S3-Touch-LCD-5-Demo.zip
- LVGL 8.3.11: MIT, downloaded by ESP-IDF component manager.
- Espressif GT911 touch driver: downloaded by ESP-IDF component manager;
  exact transitive versions recorded in `firmware/dependencies.lock`.
- Archivo and DM Sans: SIL Open Font License 1.1. Licenses are in
  `firmware/main/fonts/`. Upstream: https://github.com/google/fonts
- Font converter: `lv_font_conv` 1.5.3 (MIT), used only during asset generation.
- Fraunces: SIL Open Font License 1.1, by Undercase Type. Source:
  https://github.com/google/fonts/tree/main/ofl/fraunces. License:
  `firmware/main/fonts/Fraunces-OFL.txt`. Hello uses weight 500, optical size
  72, softness 100, and wonk 0. The 144/128px headline subsets and 84/56/40px
  guest text subsets use lossless compressed 4 bpp LVGL bitmaps.

Firmware font files are generated 4 bpp subsets of Archivo 900, normal width,
and DM Sans 400, optical size 24. Dynamic guest-text sizes contain 319 supported
characters, including accented Latin letters and common punctuation; other
sizes retain ASCII 32–126. `scripts/generate-guest-fonts.py` generates the
shared validation list from actual font coverage. Source fonts are retained
locally in `.local/fonts/`; compiled font assets are included with source.

The application source has no project-wide license selected yet. Choose one
before public distribution; third-party notices and licenses remain applicable.
