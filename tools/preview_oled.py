#!/usr/bin/env python3
"""Build the visual contact sheet from the production-renderer PBMs (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parent.parent
names = ['running', 'pending', 'warning', 'off', 'overpower', 'fault']
canvas = Image.new('RGB', (384 * len(names), 224), '#202020')
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 14)
for index, name in enumerate(names):
    image = Image.open(root / f'tests/ui-previews/{name}.pbm').convert('RGB')
    canvas.paste(image.resize((384, 192), Image.Resampling.NEAREST), (index * 384, 32))
    draw.text((index * 384 + 8, 8), name, fill='white', font=font)
canvas.save(root / 'tests/ui-previews/screens.png')
