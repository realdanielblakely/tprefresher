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

The firmware uses TFT_eSPI with the ST7796 driver and XPT2046_Touchscreen.

Orientation is rotated 180 degrees (tft.setRotation(2)) so the USB cable exits the
top of the board and does not block a stand. Touch mapping is inverted on BOTH axes
to match: X because of that rotation, Y because the panel runs opposite the display
regardless. If you change the rotation, change both axis mappings with it, and
verify against the logged touch coordinates before assuming it is right.

## Current state, verified on hardware 2026-09-06

Working and confirmed on the physical board: display, resistive touch on all three
cards, Wi-Fi, five second polling, over-the-air updates, and UDP log streaming.
The board runs on wall power away from any computer and is updated over Wi-Fi.

Not done: the backend has no durable home. It runs by hand on a laptop, so the
board shows OFFLINE whenever that laptop sleeps. That is the next real task.

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
2. Set Wi-Fi credentials, any API token, and an OTA password in secrets.h.
3. API_BASE_URL must use the backend LAN IP and port. localhost on the ESP32 means the ESP32 itself, not the computer running the server.

First flash, or any recovery flash, goes over USB:

    pio run -e usb -t upload --upload-port /dev/cu.usbserial-110

After that, update over Wi-Fi. The board registers as tprefresher.local:

    export OTA_PASSWORD=<the value from secrets.h>
    pio run -e ota -t upload

espota cannot resolve .local itself on some setups. If the upload reports the host
was not found, pass the address instead: --upload-port <board ip>.

## Watching the board without a cable

Log lines are mirrored to UDP port 4444 as a broadcast, so the board stays
debuggable once it is mounted somewhere inconvenient. Listen with:

    python3 -c "import socket;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('',4444))
    while 1: d,a=s.recvfrom(512);print(a[0],d.decode().strip())"

## Done versus not done

- Backend, browser UI, and firmware are implemented and running on real hardware.
- Firmware compiles and has been flashed. Display, resistive touch, Wi-Fi, live
  polling, and over-the-air updates are all confirmed working on the board.
- Touch is calibrated. The panel Y axis runs opposite the display, so the mapping
  is inverted on purpose. Do not "fix" it back.
- The backend still has no durable deployment. It runs by hand on a laptop, which
  means the board goes OFFLINE whenever that machine sleeps.
- Both the board and the backend host take addresses from DHCP. The board does not
  care, because it is reachable as tprefresher.local. The backend host address is
  compiled into the firmware, so if it moves the board cannot find it.

## Owner preferences

- Keep solutions practical, free, and self-hostable.
- Avoid AI-isms and em dashes in documentation.
- Never commit secrets, including firmware/include/secrets.h, Wi-Fi credentials, or API tokens.

## Good next tasks

1. Deploy the backend on a host that stays awake, with durable disk storage.
2. Give that host a DHCP reservation, or the compiled-in API_BASE_URL goes stale.
3. Make the server address settable on the board instead of compiled in, so moving
   the backend does not require a firmware push.
4. Rename the bathroom rooms and display labels if household names differ.
5. Add Home Assistant integration. GET /api/status works as a REST sensor with no
   changes. Driving flags from HA is cleaner over MQTT.
6. Print QR codes for the GET flag routes and put them in the bathrooms.
7. Enable and document API_TOKEN for deployments beyond a trusted LAN.

## Pitfalls

- Do not substitute the 2.4-inch CYD pinout; this board uses the LCDWiki 4-inch pinout above.
- The touch controller shares SPI and has its own CS and IRQ pins; do not assume common CYD touch wiring.
- The ESP32 must reach the backend over Wi-Fi; localhost is not the development computer.
- backend/data.json must live on durable disk if the server is hosted or restarted.
- Never commit local secrets or generated state.
- macOS 15 and later gate local network access per application. If the Mac can ping
  the router but no other device on the LAN, and sends fail with "no route to host"
  while the routing table and ARP entries look healthy, the terminal application
  needs Local Network permission in Privacy and Security settings. This looks
  exactly like a board fault and is not one.
- Wi-Fi power save is disabled in firmware on purpose. With it on, the board still
  makes outbound requests but drops inbound packets, so OTA and ping fail.
- The addApbChangeCallback duplicate warning at boot is harmless. It comes from
  TFT_eSPI and SPI both registering the same callback.
