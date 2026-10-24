"""Pack the existing logo artwork as RGB565 for the native PAX home header.

Requires Pillow; run manually only when replacing the source logo.
The source PNG is left intact. Output is compiled app pixel data.
"""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1]
im = Image.open(root/'assets/branding/meshmatsu-logo-dark-v1.png').convert('RGB').resize((264,88),Image.Resampling.LANCZOS)
values = [((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in im.get_flattened_data()]
lines = ['// Generated from assets/branding/meshmatsu-logo-dark-v1.png; do not hand-edit.', '#include <stdint.h>', 'static const uint16_t mesh_brand_pixels[264 * 88] = {']
lines += [','.join(f'0x{v:04x}' for v in values[i:i+16])+',' for i in range(0,len(values),16)]
lines += ['};','']
(root/'main/brand_logo_pixels.inc').write_text('\n'.join(lines))
