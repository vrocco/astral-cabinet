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

The readings are reflective entertainment, not predictions or medical/financial advice.

## Hardware

Target: common ESP32-2432S028 CYD board:

- ILI9341 320x240 TFT: MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST 4
- XPT2046 touch controller: CS 33, IRQ 36
- Backlight: GPIO 21

Some CYD revisions vary. If touch is offset, adjust `TOUCH_X_MIN/MAX` and `TOUCH_Y_MIN/MAX` in `src/main.cpp`.

## Build and flash

Install PlatformIO, then from this directory:

```sh
pio run
pio run -t upload
pio device monitor
```

No network credentials are needed. The **Cabinet** screen stores a selectable guest profile in ESP32 Preferences; the profile editor and arbitrary typed names are planned for a later iteration.

## Controls

Tap the large buttons. On the reading screen, tap each face-down card to reveal it. The top-left corner returns to the main menu.

## License

MIT. Tarot text is original project content.
