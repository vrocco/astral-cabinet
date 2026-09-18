#!/usr/bin/env python3
"""Create CYD-ready JPEG tarot art and an inspection contact sheet.

The source images are generated at high resolution. The target board displays
card art at 120x160 and spread thumbnails at 78x104, so keeping source-sized
PNGs on the SD card wastes space and slows decoding.
"""
import argparse
from pathlib import Path
import re
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "sd" / "astral"
OUT.mkdir(parents=True, exist_ok=True)

FULL_SIZE = (120, 160)
THUMB_SIZE = (78, 104)


def resize_crop(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Center-crop to exact 3:4 output while preserving the card composition."""
    target_ratio = size[0] / size[1]
    source_ratio = image.width / image.height
    if source_ratio > target_ratio:
        crop_w = round(image.height * target_ratio)
        left = (image.width - crop_w) // 2
        image = image.crop((left, 0, left + crop_w, image.height))
    else:
        crop_h = round(image.width / target_ratio)
        top = (image.height - crop_h) // 2
        image = image.crop((0, top, image.width, top + crop_h))
    return image.resize(size, Image.Resampling.LANCZOS)


parser = argparse.ArgumentParser(description="Make CYD SD card tarot JPEGs from source artwork.")
parser.add_argument("source_dir", type=Path, help="Directory containing 00.png..21.png and card_back.png")
args = parser.parse_args()

sources = sorted(
    (path for path in args.source_dir.iterdir() if path.suffix.lower() in {".jpg", ".jpeg", ".png"}),
    key=lambda path: (path.stem == "card_back", path.stem),
)
if not sources:
    raise SystemExit(f"No PNG or JPEG source art found in {args.source_dir}")

cards: list[tuple[str, Image.Image]] = []
asset_count = 0
for source in sources:
    name = source.stem
    if not (name == "card_back" or re.fullmatch(r"(?:0[0-9]|1[0-9]|2[0-1])", name)):
        raise SystemExit(f"Unexpected source name {source.name}; use 00.png through 21.png or card_back.png")
    with Image.open(source) as raw:
        card = resize_crop(raw.convert("RGB"), FULL_SIZE)
    card.save(OUT / f"{name}.jpg", "JPEG", quality=91, optimize=True, progressive=False)
    thumb = card.resize(THUMB_SIZE, Image.Resampling.LANCZOS)
    thumb.save(OUT / f"{name}_s.jpg", "JPEG", quality=88, optimize=True, progressive=False)
    asset_count += 2
    if name != "card_back":
        card.transpose(Image.Transpose.ROTATE_180).save(
            OUT / f"{name}_r.jpg", "JPEG", quality=91, optimize=True, progressive=False
        )
        thumb.transpose(Image.Transpose.ROTATE_180).save(
            OUT / f"{name}_rs.jpg", "JPEG", quality=88, optimize=True, progressive=False
        )
        asset_count += 2
    cards.append((name, card))

# A four-column contact sheet is easy to inspect at a glance before copying assets to SD.
columns = 4
rows = (len(cards) + columns - 1) // columns
canvas = Image.new("RGB", (columns * 150, rows * 205), (10, 18, 42))
draw = ImageDraw.Draw(canvas)
for index, (name, card) in enumerate(cards):
    x = (index % columns) * 150 + 15
    y = (index // columns) * 205 + 18
    canvas.paste(card, (x, y))
    draw.text((x, y + 168), name.replace("_", " ").upper(), fill=(238, 209, 126))
canvas.save(ROOT / "assets" / "art-preview.jpg", "JPEG", quality=92, optimize=True)

print(f"Wrote {asset_count} SD JPEGs to {OUT}")
print(f"Preview: {ROOT / 'assets' / 'art-preview.jpg'}")
