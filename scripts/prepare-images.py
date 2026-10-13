"""Export curated sources and their real three transport-nearest neighbors for both UIs."""
from pathlib import Path
from PIL import Image, ImageOps
import json
import re
from io import BytesIO

ROOT = Path(__file__).resolve().parents[1]
DRIFT = ROOT.parent / 'perceptual-palette-drift/docs/constellation'
METRIC = 'transport_delta_e'
SPREADS = [
    ('Afterglow', '#d5a4bd', 'raw/2025/_DSC8824.jpg'),
    ('Crimson', '#df746b', 'raw/2020/_DSC3226.jpg'),
    ('Blue hour', '#9bb9e8', 'raw/2025/_DSC9578.jpg'),
    ('Silver', '#c9cdc9', 'raw/2024/_DSC6537.jpg'),
]
match = re.search(r'const DATA = (\{.*?\});', (DRIFT / 'index.html').read_text())
if not match:
    raise RuntimeError('Palette Drift DATA manifest not found')
data = json.loads(match.group(1))
nodes = {node['image']: node for node in data['nodes']}

def native_image(name, img, alpha=False, jpeg=False):
    if jpeg:
        encoded = BytesIO()
        img.save(encoded, format='JPEG', quality=92, subsampling=0, progressive=False)
        pixels = encoded.getvalue()
        lines = [','.join(f'0x{x:02x}' for x in pixels[i:i + 32]) for i in range(0, len(pixels), 32)]
        (ROOT / f'firmware/main/images/{name}.c').write_text(
            '#include "lvgl.h"\nstatic const uint8_t pixels[]={\n' + ',\n'.join(lines) +
            '\n};\nconst lv_img_dsc_t pet_img_' + name + '={.header.cf=LV_IMG_CF_RAW,' +
            f'.header.w={img.width},.header.h={img.height},.data_size=sizeof(pixels),.data=pixels}};\n')
        return
    rgba = img.convert('RGBA').tobytes()
    pixels = bytearray()
    for offset in range(0, len(rgba), 4):
        r, g, b, a = rgba[offset:offset + 4]
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        pixels.extend((value & 255, value >> 8))
        if alpha:
            pixels.append(a)
    lines = [','.join(f'0x{x:02x}' for x in pixels[i:i + 32]) for i in range(0, len(pixels), 32)]
    (ROOT / f'firmware/main/images/{name}.c').write_text(
        '#include "lvgl.h"\nstatic const uint8_t pixels[]={\n' + ',\n'.join(lines) +
        '\n};\nconst lv_img_dsc_t pet_img_' + name + '={.header.cf=' +
        ('LV_IMG_CF_TRUE_COLOR_ALPHA,' if alpha else 'LV_IMG_CF_TRUE_COLOR,') +
        f'.header.w={img.width},.header.h={img.height},.data_size=sizeof(pixels),.data=pixels}};\n')

spreads = []
for si, (title, accent, source) in enumerate(SPREADS):
    neighbors = sorted(data['neighbors'][source][METRIC], key=lambda e: e['distance'])[:3]
    assert len(neighbors) == 3 and len({e['target'] for e in neighbors}) == 3
    originals = []
    for edge in [{'target': source}, *neighbors]:
        original_path = ROOT.parent / 'perceptual-palette-drift/data' / nodes[edge['target']]['image']
        originals.append(ImageOps.exif_transpose(Image.open(original_path)).convert('RGB'))
    manifest = []
    for index, edge in enumerate([{'target': source, 'rank': 0, 'distance': None}, *neighbors]):
        node = nodes[edge['target']]
        name = f'palette_{si * 4 + index + 1}'
        path = ROOT.parent / 'perceptual-palette-drift/data' / node['image']
        if not path.is_file():
            raise FileNotFoundError(f'Unwatermarked original missing: {path}')
        original = originals[index]
        bounds = (168, 252)  # Uniform 2:3 preview crop; inspection preserves the original.
        img = ImageOps.fit(original, bounds, Image.Resampling.LANCZOS)
        browser = ImageOps.fit(original, tuple(v * 2 for v in bounds), Image.Resampling.LANCZOS)
        browser.save(ROOT / f'design/assets/{name}.png')
        native_image(name, img, jpeg=True)
        detail = ImageOps.contain(original, (392, 392), Image.Resampling.LANCZOS)
        native_image(name + '_detail', detail, jpeg=True)
        browser_detail = ImageOps.contain(original, (1136, 880), Image.Resampling.LANCZOS)
        browser_detail.save(ROOT / f'design/assets/{name}_detail.jpg', quality=94)
        manifest.append({
            'asset': name, 'src': f'assets/{name}.png', 'source': str(path.resolve()),
            'detailSrc': f'assets/{name}_detail.jpg', 'detailDimensions': list(detail.size),
            'image': node['image'], 'id': node['id'], 'year': node['year'],
            'rank': index, 'distance': edge['distance'],
            'dimensions': list(img.size), 'browserDimensions': list(browser.size),
            'operation': 'Unwatermarked original; centered 2:3 crop for overview; separate uncropped aspect-preserving inspection image; baseline JPEG on device',
            'palette': [{'hex': p['hex'], 'weight': p['weight']} for p in node['palette']],
        })
    spreads.append({'name': title, 'accent': accent, 'source': manifest[0], 'neighbors': manifest[1:]})

selection = {'metric': METRIC, 'spreads': spreads}
(ROOT / 'design/assets/provenance.json').write_text(json.dumps(selection, indent=2) + '\n')
(ROOT / 'design/assets/constellation-data.js').write_text(
    '// Generated from Palette Drift. Each source has its actual three nearest neighbors.\n' +
    'window.PET_CONSTELLATION = ' + json.dumps(selection, indent=2) + ';\n')
logo = Image.open(ROOT / 'design/assets/atla-logo-source.png').convert('RGBA')
logo = logo.crop(logo.getbbox())
logo.save(ROOT / 'design/assets/atla-logo.png')
logo = ImageOps.contain(logo, (400, 270), Image.Resampling.LANCZOS)
native_image('atla', logo, alpha=True)
with (ROOT / 'firmware/main/images/pet_images.h').open('w') as out:
    out.write('// Generated by scripts/prepare-images.py.\n#pragma once\n#include "lvgl.h"\n#define PET_SPREAD_COUNT 4\n')
    out.write('LV_IMG_DECLARE(pet_img_atla);\n')
    for s in spreads:
        for item in [s['source'], *s['neighbors']]:
            out.write(f'LV_IMG_DECLARE(pet_img_{item["asset"]});\n')
            out.write(f'LV_IMG_DECLARE(pet_img_{item["asset"]}_detail);\n')
    out.write('typedef struct { const char *name; uint32_t accent; const lv_img_dsc_t *images[4]; const lv_img_dsc_t *details[4]; const char *captions[4]; const char *distances[3]; uint32_t colors[4][5]; uint16_t weights[4][5]; } pet_spread_t;\n')
    out.write('static const pet_spread_t pet_spreads[PET_SPREAD_COUNT] = {\n')
    for s in spreads:
        items = [s['source'], *s['neighbors']]
        out.write('{.name=' + json.dumps(s['name']) + ',.accent=0x' + s['accent'][1:] + ',\n.images={')
        out.write(','.join('&pet_img_' + p['asset'] for p in items) + '},\n.details={')
        out.write(','.join('&pet_img_' + p['asset'] + '_detail' for p in items) + '},\n.captions={')
        out.write(','.join(json.dumps(f'{p["id"]} / {p["year"]}') for p in items) + '},\n.distances={')
        out.write(','.join(json.dumps(f'{p["distance"]:.2f}') for p in items[1:]) + '},\n.colors={')
        for p in items: out.write('{' + ','.join('0x' + c['hex'][1:] for c in p['palette']) + '},')
        out.write('},\n.weights={')
        for p in items:
            weights = [round(c['weight'] * 10000) for c in p['palette']]
            weights[0] += 10000 - sum(weights)
            out.write('{' + ','.join(map(str, weights)) + '},')
        out.write('}},\n')
    out.write('};\n')
for s in spreads:
    print(s['name'], s['source']['id'], [(n['id'], n['distance']) for n in s['neighbors']])
