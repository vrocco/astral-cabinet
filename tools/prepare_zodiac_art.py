#!/usr/bin/env python3
"""Create CYD-ready square zodiac medallions from high-resolution source art."""
import argparse
from pathlib import Path
import re
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "sd" / "astral" / "zodiac"
OUT.mkdir(parents=True, exist_ok=True)
SIZE = (60, 60)


def center_square(image: Image.Image) -> Image.Image:
    edge = min(image.width, image.height)
    left = (image.width - edge) // 2
    top = (image.height - edge) // 2
    return image.crop((left, top, left + edge, top + edge)).resize(SIZE, Image.Resampling.LANCZOS)


parser = argparse.ArgumentParser(description="Make CYD 60x60 zodiac JPEG medallions.")
parser.add_argument("source_dir", type=Path, help="Directory containing 00.png through 11.png")
args = parser.parse_args()

sources = sorted(
    (path for path in args.source_dir.iterdir() if path.suffix.lower() in {".jpg", ".jpeg", ".png"}),
    key=lambda path: path.stem,
)
if len(sources) != 12:
    raise SystemExit(f"Expected 12 source images in {args.source_dir}; found {len(sources)}")

medallions: list[tuple[str, Image.Image]] = []
for source in sources:
    if not re.fullmatch(r"(?:0[0-9]|1[0-1])", source.stem):
        raise SystemExit(f"Unexpected source name {source.name}; use 00.png through 11.png")
    with Image.open(source) as raw:
        medallion = center_square(raw.convert("RGB"))
    medallion.save(OUT / f"{source.stem}.jpg", "JPEG", quality=91, optimize=True, progressive=False)
    medallions.append((source.stem, medallion))

columns = 4
canvas = Image.new("RGB", (columns * 90, 3 * 90), (10, 18, 42))
draw = ImageDraw.Draw(canvas)
for index, (name, medallion) in enumerate(medallions):
    x = (index % columns) * 90 + 15
    y = (index // columns) * 90 + 14
    canvas.paste(medallion, (x, y))
    draw.text((x + 20, y + 65), name, fill=(238, 209, 126))
canvas.save(ROOT / "assets" / "zodiac-preview.jpg", "JPEG", quality=92, optimize=True)

print(f"Wrote {len(medallions)} zodiac JPEGs to {OUT}")
print(f"Preview: {ROOT / 'assets' / 'zodiac-preview.jpg'}")
