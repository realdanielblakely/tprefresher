# Claude Code Handoff

## What this project is

TP Refresher is a closet toilet-paper status board. A phone page flags a bathroom when it needs paper, an ESP32 display shows the current status, and tapping a flagged display card confirms the restock. Flags are amber for 24 hours, then become red and overdue.

## Hardware

- Board: AITRIP 4-inch ESP32-32E / E32R40T
- Display: 320x480 ST7796S LCD
- Touch: resistive touch panel
- Use the LCDWiki 4-inch ESP32-32E pinout. This is not the 2.4-inch CYD pinout.
- LCD: TFT MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, backlight 27
- Touch: shared SPI, CS 33, IRQ 36

The firmware uses TFT_eSPI with the ST7796 driver and XPT2046_Touchscreen. Touch panels can vary, so calibration may be needed.

## Repository layout
- backend/: Express server, configuration, durable state, and API
- public/: browser phone UI
- firmware/: PlatformIO ESP32 display firmware
- package.json: project metadata and start script
## API and configuration

| Method | Route | Purpose |
| --- | --- | --- |
| GET | /api/status | Return all bathroom statuses |
| POST | /api/bathrooms/:id/flag | Flag a bathroom |
| GET | /api/bathrooms/:id/flag | Bookmark or QR flag action |
| POST | /api/bathrooms/:id/confirm | Clear a bathroom flag after restocking |

API_TOKEN is optional. If set, flag and confirm require x-api-token or a Bearer authorization header.
State is stored in backend/data.json on durable storage. PORT selects the HTTP port and defaults to 3000.

## Running locally

Install dependencies and run the project start script.
The phone UI is on port 3000 by default. For the ESP32, use the host computer LAN IP, not localhost.

## Flashing the firmware

1. Copy firmware/include/secrets.h.example to firmware/include/secrets.h.
2. Set Wi-Fi credentials and any API token in secrets.h.
3. Use PlatformIO to upload the firmware over USB.
4. API_BASE_URL must use the backend LAN IP and port. localhost on the ESP32 means the ESP32 itself, not the computer running the server.

## Done versus not done

- Backend, browser UI, and firmware are implemented and pushed.
- PlatformIO has never been compiled on the agent box; compile and upload on a machine with the board and toolchain available.
- Resistive touch may need calibration for the specific panel.
- There is no production deployment yet.

## Owner preferences

- Keep solutions practical, free, and self-hostable.
- Avoid AI-isms and em dashes in documentation.
- Never commit secrets, including firmware/include/secrets.h, Wi-Fi credentials, or API tokens.

## Good next tasks

1. Rename the bathroom rooms and display labels if household names differ.
2. Compile the firmware with PlatformIO and fix board or library issues.
3. Calibrate resistive touch coordinates and axis transforms.
4. Deploy the backend on a LAN host with durable disk storage.
5. Add Home Assistant integration and printed flag QR codes.
6. Enable and document API_TOKEN for deployments beyond a trusted LAN.

## Pitfalls

- Do not substitute the 2.4-inch CYD pinout; this board uses the LCDWiki 4-inch pinout above.
- The touch controller shares SPI and has its own CS and IRQ pins; do not assume common CYD touch wiring.
- The ESP32 must reach the backend over Wi-Fi; localhost is not the development computer.
- backend/data.json must live on durable disk if the server is hosted or restarted.
- Never commit local secrets or generated state.
