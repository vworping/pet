"""Export only characters present in both fonts; validation uses that same set."""
from pathlib import Path
from fontTools.ttLib import TTFont
import subprocess
ROOT=Path(__file__).resolve().parents[1]
a=TTFont(ROOT/'.local/fonts/Archivo-static.ttf').getBestCmap()
d=TTFont(ROOT/'.local/fonts/DMSans-static.ttf').getBestCmap()
keys=sorted(k for k in a.keys()&d.keys() if 32<=k<=126 or 160<=k<=591 or k in [0x2013,0x2014,0x2018,0x2019,0x201c,0x201d,0x2026])
(ROOT/'firmware/main/pet_glyphs.h').write_text('// Generated from both bundled fonts.\n#pragma once\n#include <stdint.h>\nstatic const uint16_t pet_supported_glyphs[]={'+','.join(map(str,keys))+'};\n')
converter=ROOT/'.local/font-tools/node_modules/lv_font_conv/lv_font_conv.js'
for family,sizes in [('Archivo',[72]),('DMSans',[18,24,28])]:
 for size in sizes:
  name='pet_archivo' if family=='Archivo' else 'pet_dm'
  subprocess.run(['node',str(converter),'--font',str(ROOT/f'.local/fonts/{family}-static.ttf'),'--size',str(size),'--bpp','4','--format','lvgl','--range',','.join(str(k) for k in keys),'--no-compress','--output',str(ROOT/f'firmware/main/fonts/{name}_{size}.c')],check=True)
print('Generated',len(keys),'supported characters, including accented names and curly punctuation.')

# The phone validates against the exact same set of glyphs as the native badge.
import re
page=ROOT/'firmware/main/guest.html'
page.write_text(re.sub(r'const supported=new Set\(\[.*?\]\);', 'const supported=new Set(['+','.join(map(str,keys))+']);', page.read_text()))
