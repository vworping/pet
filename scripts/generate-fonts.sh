#!/bin/bash
set -euo pipefail
PET_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PET_ROOT"
PET_CONVERTER="$PET_ROOT/.local/font-tools/node_modules/lv_font_conv/lv_font_conv.js"
PET_GUEST_RANGE="$(python3 -c 'from pathlib import Path;print(Path("firmware/main/pet_glyphs.h").read_text().split("[]={")[1].split("}")[0])')"
for PET_SIZE in 104 72 32; do
    PET_RANGE=32-126
    if [[ "$PET_SIZE" == 72 ]]; then PET_RANGE="$PET_GUEST_RANGE"; fi
    node "$PET_CONVERTER" --font .local/fonts/Archivo-static.ttf --size "$PET_SIZE" --bpp 4 --format lvgl --range "$PET_RANGE" --no-compress --output "firmware/main/fonts/pet_archivo_${PET_SIZE}.c"
done
node "$PET_CONVERTER" --font .local/fonts/Archivo-static.ttf --size 192 --bpp 4 --format lvgl --symbols "[pet.]" --no-compress --output firmware/main/fonts/pet_archivo_192.c
for PET_SIZE in 18 24 28 36; do
    PET_RANGE="$PET_GUEST_RANGE"
    if [[ "$PET_SIZE" == 36 ]]; then PET_RANGE=32-126; fi
    node "$PET_CONVERTER" --font .local/fonts/DMSans-static.ttf --size "$PET_SIZE" --bpp 4 --format lvgl --range "$PET_RANGE" --no-compress --output "firmware/main/fonts/pet_dm_${PET_SIZE}.c"
done

# Medium optical-size DM Sans for the animated multilingual greeting.
python3 - <<'PYFONT'
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
font = TTFont('.local/fonts/DMSans.ttf')
instantiateVariableFont(font, {'wght': 500, 'opsz': 40}, inplace=True).save('.local/fonts/DMSans-greeting.ttf')
PYFONT
node "$PET_CONVERTER" --font .local/fonts/DMSans-greeting.ttf --size 96 --bpp 4 --format lvgl --symbols "[]hellohiheyhowdyholabuenassalutbonjourcoucou" --no-compress --output firmware/main/fonts/pet_greeting_96.c
node "$PET_CONVERTER" --font .local/fonts/DMSans-greeting.ttf --size 128 --bpp 4 --format lvgl --symbols "[]hellohiheyhowdyholabuenassalutbonjourcoucou" --no-compress --output firmware/main/fonts/pet_greeting_128.c

# Soft serif headlines for Hello, with the full guest character set for names.
# Source: google/fonts/ofl/fraunces (SIL Open Font License).
python3 - <<'PYFONT'
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
font = TTFont('.local/fonts/Fraunces.ttf')
instantiateVariableFont(font, {'wght': 500, 'opsz': 72, 'SOFT': 100, 'WONK': 0}, inplace=True).save('.local/fonts/Fraunces-static.ttf')
PYFONT
node "$PET_CONVERTER" --font .local/fonts/Fraunces-static.ttf --size 144 --bpp 4 --format lvgl --symbols "A small hello.Hello...Say what?" --output firmware/main/fonts/pet_fraunces_144.c
node "$PET_CONVERTER" --font .local/fonts/Fraunces-static.ttf --size 128 --bpp 4 --format lvgl --symbols "A small hello.[pet.]" --output firmware/main/fonts/pet_fraunces_128.c
for PET_SIZE in 84 56 40; do
    node "$PET_CONVERTER" --font .local/fonts/Fraunces-static.ttf --size "$PET_SIZE" --bpp 4 --format lvgl --range "$PET_GUEST_RANGE" --output "firmware/main/fonts/pet_fraunces_${PET_SIZE}.c"
done

# Contrasting weights for the introduction title.
python3 - <<'PYFONT'
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
for weight in (400,700):
    font=TTFont('.local/fonts/Fraunces.ttf')
    instantiateVariableFont(font, {'wght':weight,'opsz':72,'SOFT':100,'WONK':0}, inplace=True).save(f'.local/fonts/Fraunces-{weight}.ttf')
PYFONT
node "$PET_CONVERTER" --font .local/fonts/Fraunces-400.ttf --size 72 --bpp 4 --format lvgl --symbols "Ahello. sayitback?Onemnt" --output firmware/main/fonts/pet_hello_regular_72.c
node "$PET_CONVERTER" --font .local/fonts/Fraunces-700.ttf --size 144 --bpp 4 --format lvgl --symbols "small" --output firmware/main/fonts/pet_hello_bold_144.c
