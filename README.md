# The Astral Cabinet

A portable, offline tarot and zodiac fortune-teller for the ESP32-2432S028 CYD (320x240 ILI9341 touchscreen).

## First build

- Art Nouveau-inspired midnight-blue, burgundy, and antique-gold interface
- 22 Major Arcana cards with upright/reversed meanings
- Zodiac sign selection and profile panel
- Daily one-card reading
- Three-card Past / Present / Becoming spread
- Offline tarot and zodiac experience, plus an online Live Astral doorway
- NTP-synchronized local time, calculated moon phase, illumination, and next lunar thresholds
- Daily cached planetary signs, retrograde states, and notable aspects when connected
- Custom name and welcome message stored in ESP32 Preferences
- Touch-driven card reveal with simple ritual animations
- No Wi-Fi required
- A Settings gear on the doorway screen with a confirmed reset of saved Wi-Fi

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
/astral/doorways.jpg
/astral/settings_gear.jpg
/astral/online.jpg
/astral/setup.html
/astral/live_astral.jpg
/astral/sky_now.jpg
/astral/ritual_calendar.jpg
/astral/cosmic_weather.jpg
/astral/elemental_ritual.jpg
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
is the full-screen Offline/Online choice screen; and `doorways.jpg` is the
four-portal Offline menu. `online.jpg` is the Wi-Fi setup screen. Full card
images are 120×160 JPEGs and spread thumbnails are 78×104 JPEGs. Every
Major Arcana card also has pre-rotated 180° full and thumbnail assets for a
genuine reversed draw. Twelve 60×60 illustrated zodiac medallions live under
`/astral/zodiac/`. This keeps every asset small and decoded quickly while
delivering an illustrated Art Nouveau deck; the line-motif renderer remains
only as a safe fallback when the SD card is absent or unreadable.

`setup.html` is the themed Wi-Fi configuration page served by the device's
setup AP. It deliberately lives on the SD card rather than in firmware flash.
If the card is unavailable, a minimal firmware-resident recovery form remains
available so Wi-Fi onboarding still works.

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

### Display calibration across CYD revisions

The 2.8-inch CYD label does not guarantee an identical LCD panel initialization.
The firmware keeps the original panel profile by default and includes eight
per-device display profiles covering the known ILI9341-compatible combinations
of legacy/standard gamma, BGR/RGB channel order, and display inversion.

On a device whose artwork looks washed out, blue-white, inverted, or has swapped
red/blue colors, open the doorway-screen gear, then select **Display
Calibration**. Use **Try Next** until the red, green, blue, and white swatches
look correct and the navy sample screen looks rich rather than washed out. Tap
**Save This** to store that profile in the device's Preferences; each CYD keeps
its own selection through reboots and future firmware flashes unless its flash
is erased.

## Build and flash

Install PlatformIO, then from this directory:

```sh
pio run
pio run -t upload
pio device monitor
```

This build uses the full 4 MB flash for one factory application instead of
reserving a second OTA application slot. The device does not offer OTA firmware
updates, so this makes room for its Wi-Fi/TLS client without reducing a feature
that was available before. A normal PlatformIO upload writes the matching
partition table and application together.

No network credentials are needed to flash the device. The **Cabinet** screen stores a selectable guest profile in ESP32 Preferences; the profile editor and arbitrary typed names are planned for a later iteration.

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

The same tool can prepare a replacement doorway menu background:

```sh
python3 tools/prepare_boot_art.py /path/to/doorway-art.png --output assets/sd/astral/doorways.jpg
```

For the online setup background:

```sh
python3 tools/prepare_boot_art.py /path/to/online-art.png --output assets/sd/astral/online.jpg
```

## Online setup

If the device does not have a working internet connection, choose **Online** on
the journey screen. It starts an open Wi-Fi access point named `astral` at
`192.168.4.1` and shows the same instructions on the display.

1. Connect a phone or computer to Wi-Fi network `astral`.
2. Open `http://192.168.4.1`.
3. Choose a scanned nearby network, or check **Enter my network name manually**
   for a hidden network. Provide its password and choose a timezone.
4. Location is optional and blank by default. Enter a ZIP code only if you
   choose to provide it.
5. Select **Save and restart**.

The selected SSID and password are stored in ESP32 Preferences and used after
reboot. On the next tap from the boot artwork, the firmware waits briefly for
the saved Wi-Fi and tests internet reachability; if reachable it opens the
four-portal menu directly, otherwise it returns to the Offline/Online screen.

To set up a different Wi-Fi network later, tap the small gold gear in the
bottom-right of the main doorway screen. Choose **Delete saved Wi-Fi**, then
confirm **Delete & reboot**. The device clears its saved network and online
setup details, restarts, and returns to first-time onboarding.

The setup access point is intentionally open so a new owner can connect without
prior credentials. Perform setup away from untrusted nearby users.

## Live Astral

The main menu always keeps **Zodiac**. When offline, the fourth doorway is
**Elemental Ritual**: a locally generated reflection for the selected sign's
Fire, Earth, Air, or Water element. Touch its text panel to rotate among three
offline ritual prompts. When online, that doorway becomes **Live Astral** and
opens three connected experiences illustrated with their own Art Nouveau
backgrounds:

- **Sky Now** shows NTP-synchronized local time, current lunar phase and
  illumination, plus the next new and full moon.
- **Ritual Calendar** keeps the next lunar thresholds visible without turning
  the cabinet into a generic calendar.
- **Cosmic Weather** displays Mercury, Venus, Mars, and Jupiter signs and
  direct/retrograde state, plus a major aspect when one is close.

The device fetches a compact non-personal daily cache from this repository's
`data/cosmic.txt`, stores it at `/astral/cosmic.txt`, and continues to display
the last successful cache if the network goes away. GitHub Actions regenerates
the cache daily from PyEphem via `.github/workflows/update-cosmic-feed.yml`.
The feed has no API key and contains only public astronomical data. The ESP32
uses an HTTPS request with certificate verification disabled solely for this
non-sensitive display cache; Wi-Fi credentials are never sent to that endpoint.
If strict certificate pinning is desired later, replace the raw GitHub endpoint
with a stable hostname and its pinned CA certificate.

## Controls

At startup, tap the illustrated Astral Cabinet entry screen, then choose a journey.
**Offline** opens a four-portal reading menu. **Online** opens the Wi-Fi setup
screen and starts the local configuration portal described above. The main menu
keeps Zodiac and uses its fourth doorway for Live Astral.
The **BACK** button on the four-option menu returns to the journey choice screen.

Tap a labeled doorway or control. A reversed draw is rendered upside down. On the tarot
reading screen, tap each face-down card to reveal it, then tap a revealed card
to read its meaning. In the Zodiac screen, tapping a medallion selects it and
opens the reading menu. The visible **BACK** button at top left returns to the
main menu from every reading screen.

## License

MIT. Tarot text is original project content.
