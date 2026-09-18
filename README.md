# The Astral Cabinet

A portable, offline tarot and zodiac fortune-teller for the ESP32-2432S028 CYD (320x240 ILI9341 touchscreen).

## First build

- Art Nouveau-inspired midnight-blue, burgundy, and antique-gold interface
- 22 Major Arcana cards with upright/reversed meanings
- Zodiac sign selection and profile panel
- Daily one-card reading
- Three-card Past / Present / Becoming spread
- Local date and calculated moon phase
- Custom name and welcome message stored in ESP32 Preferences
- Touch-driven card reveal with simple ritual animations
- No Wi-Fi required

## Illustrated deck

The CYD can render real 16-bit color JPEG artwork; the earlier line-glyph
"art" is retained only as a safe fallback when no card is inserted. This
revision introduces a proper Art Nouveau deck pipeline rather than pretending
that a few primitive drawing commands are illustrations.

Use a FAT32-formatted microSD card. Copy the repository's
`assets/sd/astral/` directory to the root of the card, so the card contains:

```text
/astral/card_back.jpg
/astral/card_back_s.jpg
/astral/boot.jpg
/astral/journey.jpg
/astral/00.jpg
/astral/00_s.jpg
/astral/00_r.jpg
/astral/00_rs.jpg
/astral/zodiac/00.jpg
...
```

For a one-step copy, `dist/astral-cabinet-sd-art.zip` contains that same
top-level `astral/` directory, plus a SHA-256 file for integrity checking.

`boot.jpg` is a full-screen 320×240 illustrated entry screen; `journey.jpg`
is the full-screen Offline/Online choice screen. Full card images are 120×160
JPEGs and spread thumbnails are 78×104 JPEGs. Every
Major Arcana card also has pre-rotated 180° full and thumbnail assets for a
genuine reversed draw. Twelve 60×60 illustrated zodiac medallions live under
`/astral/zodiac/`. This keeps every asset small and decoded quickly while
delivering an illustrated Art Nouveau deck; the line-motif renderer remains
only as a safe fallback when the SD card is absent or unreadable.

The firmware detects the card at boot. Its serial output states either
`ASTRAL: SD art ready` or `ASTRAL: SD art unavailable; using vector fallback`.

The readings are reflective entertainment, not predictions or medical/financial advice.

## Hardware

Target: common ESP32-2432S028 CYD board:

- ILI9341 320x240 TFT: MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST 4
- XPT2046 touch controller: CS 33, IRQ 36
- Backlight: GPIO 21
- microSD artwork bus: SCK 18, MISO 19, MOSI 23, CS 5 (separate HSPI bus)

Some CYD revisions vary. If touch is offset, adjust `TOUCH_X_MIN/MAX` and `TOUCH_Y_MIN/MAX` in `src/main.cpp`.

## Build and flash

Install PlatformIO, then from this directory:

```sh
pio run
pio run -t upload
pio device monitor
```

No network credentials are needed. The **Cabinet** screen stores a selectable guest profile in ESP32 Preferences; the profile editor and arbitrary typed names are planned for a later iteration.

To regenerate CYD-ready JPEGs after replacing the high-resolution source art, put
files named `00.png` through `21.png` and `card_back.png` in a directory, then
run:

```sh
python3 tools/prepare_art.py /path/to/source-art
```

For zodiac medallions, use `00.png` through `11.png` and run:

```sh
python3 tools/prepare_zodiac_art.py /path/to/zodiac-source-art
```

To replace the full-screen boot illustration, run:

```sh
python3 tools/prepare_boot_art.py /path/to/boot-art.png
```

For the journey-choice background, provide a separate output path:

```sh
python3 tools/prepare_boot_art.py /path/to/journey-art.png --output assets/sd/astral/journey.jpg
```

## Controls

At startup, tap the illustrated Astral Cabinet entry screen, then choose a journey.
**Offline** opens the current four-option reading menu. **Online** is intentionally
present but inactive until its connected experience is implemented.
The **BACK** button on the four-option menu returns to the journey choice screen.

Tap the large buttons. A reversed draw is rendered upside down. On the tarot
reading screen, tap each face-down card to reveal it, then tap a revealed card
to read its meaning. In the Zodiac screen, tapping a medallion selects it and
opens the reading menu. The visible **HOME** button at top left returns to the
main menu from every non-home screen.

## License

MIT. Tarot text is original project content.
