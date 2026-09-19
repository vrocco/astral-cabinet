#!/usr/bin/env python3
"""Lightweight wiring checks for Astral Cabinet's online experience."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text(encoding="utf-8")
portal = (root / "assets/sd/astral/setup.html").read_text(encoding="utf-8")
feed = (root / "data/cosmic.txt").read_text(encoding="utf-8").splitlines()
partitions = (root / "partitions.csv").read_text(encoding="utf-8")

required_source = (
    'configTzTime(savedTimezone.c_str(),"pool.ntp.org","time.nist.gov")',
    'netPrefs.putString("timezone",savedTimezone)',
    'netPrefs.putFloat("latitude",skyLatitude)',
    'enum Screen { BOOT, JOURNEY, ONLINE_SETUP, HOME, ZODIAC, MENU, DAILY, SPREAD, CARD, CABINET, LIVE_ASTRAL, SKY_NOW, RITUAL_CALENDAR, COSMIC_WEATHER }',
    'COSMIC_FEED_URL="https://raw.githubusercontent.com/vrocco/astral-cabinet/main/data/cosmic.txt"',
    'readSdText("/astral/setup.html")',
    'refreshCosmicCache()',
)
for item in required_source:
    assert item in source, item
for placeholder in ("{{NETWORK_OPTIONS}}", "{{TIMEZONE_OPTIONS}}", "{{LATITUDE}}", "{{LONGITUDE}}"):
    assert placeholder in portal, placeholder
assert feed[0] == "ASTRAL COSMIC WEATHER", feed[:1]
assert feed[2].startswith("Moon: "), feed[2]
assert feed[3].startswith("Next full: "), feed[3]
assert feed[4].startswith("Next new: "), feed[4]
assert "factory,  app,  factory, 0x10000,  0x3F0000," in partitions
print("Online feature wiring checks passed: 16")
