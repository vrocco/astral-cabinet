#!/usr/bin/env python3
"""Create the 320x240 Astral Cabinet boot-screen JPEG from source artwork."""

import argparse
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TARGET = (320, 240)


def resize_crop(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Center-crop an image to the target aspect ratio, then downsample cleanly."""
    target_ratio = size[0] / size[1]
    source_ratio = image.width / image.height
    if source_ratio > target_ratio:
        crop_w = round(image.height * target_ratio)
        left = (image.width - crop_w) // 2
        image = image.crop((left, 0, left + crop_w, image.height))
    elif source_ratio < target_ratio:
        crop_h = round(image.width / target_ratio)
        top = (image.height - crop_h) // 2
        image = image.crop((0, top, image.width, top + crop_h))
    return image.resize(size, Image.Resampling.LANCZOS)


parser = argparse.ArgumentParser(description="Make a 320x240 CYD full-screen background JPEG.")
parser.add_argument("source", type=Path, help="Original full-screen illustration (PNG or JPEG)")
parser.add_argument(
    "--output",
    type=Path,
    default=ROOT / "assets" / "sd" / "astral" / "boot.jpg",
    help="Output JPEG path (default: assets/sd/astral/boot.jpg)",
)
args = parser.parse_args()

if not args.source.is_file():
    raise SystemExit(f"Source artwork does not exist: {args.source}")

with Image.open(args.source) as raw:
    image = resize_crop(raw.convert("RGB"), TARGET)

output = args.output
output.parent.mkdir(parents=True, exist_ok=True)
image.save(output, "JPEG", quality=92, optimize=True, progressive=False)
print(f"Wrote {output} ({image.width}x{image.height})")
