# TP Refresher

A durable toilet-paper status board.

A phone page flags a bathroom, the closet display polls the same API, and tapping a flagged card confirms the restock. A flag is amber for 24 hours, then becomes a red high alert.
For a Claude Code handoff, see [CLAUDE.md](CLAUDE.md).

## Bathroom IDs

| ID | Display name |
| --- | --- |
| `master` | Master |
| `guest` | Guest |
| `hall` | Hall |
## Run the backend locally
Use Node.js 18 or newer.
Install dependencies and start the server with the package scripts.
The durable state file is backend/data.json.
Use DATA_FILE to select another state file.

## Phone flow
1. Open the server URL on your phone.
2. Tap Flag bathroom when a roll needs attention.
3. The display refreshes within five seconds.
4. Replace the roll and tap its display card to confirm.
5. After 24 hours without confirmation, the status is overdue and red.

GET /api/bathrooms/:id/flag is available for a one-tap bookmark or QR code.

## Firmware and flashing
The firmware is a PlatformIO project in firmware/ for the AITRIP 4-inch ESP32-32E board.
1. Install VS Code and PlatformIO.
2. Copy firmware/include/secrets.h.example to firmware/include/secrets.h.
3. Set Wi-Fi credentials and API_BASE_URL to a URL reachable by the board.
4. Connect USB-C, then Build and Upload. Monitor at 115200 baud.
The board must reach the backend over Wi-Fi. localhost means the ESP32 itself, not the computer running the server.
Use a stable USB supply; the LCD backlight and ESP32 can draw a few hundred mA.

## Display assumptions and risks
This uses the LCDWiki pinout for the AITRIP-style ESP32-32E / E32R40T board, not the common 2.4-inch CYD.
ST7796S: MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, reset EN, backlight 27.
XPT2046 touch: shared SPI, CS 33, IRQ 36.
The firmware uses TFT_eSPI ST7796_DRIVER and XPT2046_Touchscreen.
Sources: https://www.lcdwiki.com/4.0inch_ESP32-32E_Display and its product manual.
Resistive touch panels vary. Starting calibration is X 280..3860 and Y 340..3860, portrait rotation 0. If taps are offset, log raw touch values at the four corners and adjust TOUCH_*_MIN/MAX or axis transforms.

## Deployment notes
State is durable only if the host filesystem is durable. Mount persistent storage when using a hosted service.
There is no paid SaaS dependency; runtime uses Express.
The API has no user account system. Use a private LAN, reverse-proxy authentication, or API_TOKEN before exposing it publicly.

API endpoints:
GET /api/status returns all rooms with id, name, state, and flaggedAt.
POST /api/bathrooms/:id/flag marks a room as needing paper.
POST /api/bathrooms/:id/confirm clears the room.
