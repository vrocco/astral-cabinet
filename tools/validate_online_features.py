#!/usr/bin/env python3
"""Lightweight wiring checks for Astral Cabinet's online experience."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text(encoding="utf-8")
portal = (root / "assets/sd/astral/setup.html").read_text(encoding="utf-8")
feed = (root / "data/cosmic.txt").read_text(encoding="utf-8").splitlines()
partitions = (root / "partitions.csv").read_text(encoding="utf-8")
elemental_art = root / "assets/sd/astral/elemental_ritual.jpg"
settings_gear_art = root / "assets/sd/astral/settings_gear.jpg"
ritual_library = root / "assets/sd/astral/rituals.json"

required_source = (
    'configTzTime(savedTimezone.c_str(),"pool.ntp.org","time.nist.gov")',
    'netPrefs.putString("timezone",savedTimezone)',
    'netPrefs.putFloat("latitude",skyLatitude)',
    'enum Screen { BOOT, JOURNEY, ONLINE_SETUP, HOME, SETTINGS, DISPLAY_CALIBRATION, CONFIRM_NETWORK_DELETE, ZODIAC, MENU, DAILY, SPREAD, CARD, CABINET, ELEMENTAL_RITUAL, LIVE_ASTRAL, SKY_NOW, RITUAL_CALENDAR, COSMIC_WEATHER }',
    'COSMIC_FEED_URL="https://raw.githubusercontent.com/vrocco/astral-cabinet/main/data/cosmic.txt"',
    'readSdText("/astral/setup.html")',
    'refreshCosmicCache()',
    'drawSdArt("/astral/reading_menu.jpg",0,0)',
    'drawSdArt("/astral/elemental_ritual.jpg",0,0)',
    'ELEMENTAL RITUAL',
    'if(internetReady)screen=LIVE_ASTRAL; else { selectElementalRitual(); screen=ELEMENTAL_RITUAL; }',
    'x>=0 && x<65 && y>=0 && y<50',
    'drawSdArt("/astral/settings_gear.jpg",225,213)',
    'DELETE SAVED WI-FI',
    'WiFi.disconnect(true,true)',
    'netPrefs.clear()',
    'ESP.restart()',
    'applyDisplayProfile(displayProfile)',
    'DISPLAY CALIBRATION',
    'prefs.putUChar("displayProfile",displayProfile)',
    'MAXINE\'S ASTRAL CABINET',
    'ritualStep=(ritualStep+1)%3',
    'JsonDocument document;',
    'deserializeJson(document,file)',
    'SD.exists("/astral/rituals.json")',
)
for item in required_source:
    assert item in source, item
for placeholder in ("{{NETWORK_OPTIONS}}", "{{TIMEZONE_OPTIONS}}", "{{ZIPCODE}}"):
    assert placeholder in portal, placeholder
assert 'phone-location' not in portal
assert "Maxine's Astral Cabinet" in portal
assert elemental_art.exists() and elemental_art.stat().st_size > 0
assert settings_gear_art.exists() and settings_gear_art.stat().st_size > 0
assert len(json.loads(ritual_library.read_text(encoding="utf-8"))["fire"]) >= 12
assert feed[0] == "ASTRAL COSMIC WEATHER", feed[:1]
assert feed[2].startswith("Moon: "), feed[2]
assert feed[3].startswith("Next full: "), feed[3]
assert feed[4].startswith("Next new: "), feed[4]
assert "factory,  app,  factory, 0x10000,  0x3F0000," in partitions
print("Online feature wiring checks passed")
