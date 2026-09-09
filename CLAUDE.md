# Claude Code Handoff

## What this project is

TP Refresher is a closet toilet-paper status board. A phone page flags a bathroom when it needs paper, an ESP32 display shows the current status, and tapping a flagged display card confirms the restock. Flags carry a severity, and an unattended one climbs a level every 12 hours.

## Hardware

- Board: AITRIP 4-inch ESP32-32E / E32R40T
- Display: 320x480 ST7796S LCD
- Touch: resistive touch panel
- Use the LCDWiki 4-inch ESP32-32E pinout. This is not the 2.4-inch CYD pinout.
- LCD: TFT MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, backlight 27
- Touch: shared SPI, CS 33, IRQ 36

The firmware uses TFT_eSPI with the ST7796 driver and XPT2046_Touchscreen.

Orientation is rotated 180 degrees (tft.setRotation(2)) so the USB cable exits the
top of the board and does not block a stand.

Touch mapping: Y is inverted, X is not. Y because the panel runs opposite the
display. X is left alone because the 180 degree rotation cancels an inversion the
panel already had. That combination looks wrong and is correct, verified against the
tab bar. If you change the rotation, re-verify BOTH axes against logged coordinates,
and use something split left from right to check X, because a full width control
cannot tell you anything about it. See the pitfalls section.

## Housing

Enclosure brief for this AITRIP integrated board (acrylic sandwich first, then optional print): see [docs/HOUSING.md](docs/HOUSING.md).

## Current state, verified on hardware 2026-09-06

Working and confirmed on the physical board: display, resistive touch on all three
cards, Wi-Fi, five second polling, over-the-air updates, and UDP log streaming.
The board runs on wall power away from any computer and is updated over Wi-Fi.

The backend is deployed on the always-on Mac and survives process death and reboot.
Nothing needs a developer laptop to be awake any more.

## Deployment

The backend runs on the always-on Mac (user `deploy`, host `server.local`, LAN
address 192.168.1.x at time of writing) as a launchd agent.

- Working copy: `/Users/deploy/tprefresher`
- Node: `/Users/deploy/.local/bin/node`, v22
- Job: `~/Library/LaunchAgents/com.tprefresher.backend.plist`, RunAtLoad and
  KeepAlive, so launchd restarts it if the process dies
- Log: `/Users/deploy/Library/Logs/tprefresher.log`
- State: `/Users/deploy/tprefresher/backend/data.json`, survives restarts

Restart it with:

    launchctl unload ~/Library/LaunchAgents/com.tprefresher.backend.plist
    launchctl load ~/Library/LaunchAgents/com.tprefresher.backend.plist

That machine is on AC with `pmset sleep 0`, so it does not idle sleep. Closing the
lid is still a sleep, and a sleeping host serves nothing.

This is a LaunchAgent, not a LaunchDaemon, so it starts on user login rather than at
boot. After a reboot with nobody logged in, it will not be running.

The working copy was copied from a developer machine, not cloned, because the GitHub
CLI is not set up on the host. `git pull` there will need credentials before it works.
`firmware/include/secrets.h` was deliberately excluded, since the backend needs no
secrets and the firmware ones should not spread to another machine.

## Colours

The display is dark by design, not for looks. Perceived brightness on this panel is
as much about how many pixels are lit as it is about the backlight level, and the
original light grey and white layout was bright at any backlight setting.

The palette lives in constants at the top of main.cpp, built with an RGB565 macro so
they are compile time values rather than runtime calls. Change those, not the call
sites. Every text colour is light on dark: check both halves of any setTextColor
pair, since a blanket colour swap happily produces dark text on a dark background.

State colours match the Tidbyt companion exactly, including low being yellow rather
than amber, so the two displays cannot disagree about what a state looks like.

Each row carries a solid accent block ACCENT_W wide down its left edge, not a thin
stripe. The colour is the information, so it gets real area and reads from across a
room. Text starts at TEXT_X and the laundry countdown centres on CONTENT_MID, the
middle of what is left beside the block, rather than the middle of the card.

The bottom line of a paper card holds one thing, not two. When time escalated a room
it says so; otherwise it shows the tap hint. They do not fit side by side now that
the text is inset, and knowing nobody actually reported "out" matters more than a
hint you can infer by using the board twice.

## Backlight

The panel is driven by PWM on GPIO 27 through LEDC channel 0, not switched fully on.
Left at full brightness on a wall it is unpleasant, especially at night.

It idles dim and fades to full when something happens: a touch, or a real change in
a room's status, so dimming never costs you an alert. It fades back after 20 seconds.
The first touch on a dimmed screen only wakes it and is swallowed, so nobody flags a
bathroom by reaching over to read the display in the dark.

Two brightness profiles, chosen by the clock:

| | Idle | Full |
| --- | --- | --- |
| Day, 07:00 to 22:00 | 40 | 255 |
| Night, 22:00 to 07:00 | 8 | 110 |

Time comes from NTP with the timezone set by the TIMEZONE string in main.cpp,
currently America/New_York with US daylight saving rules. If the clock has not
synced, the board treats it as daytime rather than guessing dark. All of these are
constants at the top of main.cpp and are meant to be tuned.

Note the Arduino core here is 2.x, so the LEDC calls are ledcSetup, ledcAttachPin,
and ledcWrite by channel. Core 3.x replaced those with ledcAttach by pin.

## Tidbyt companion

There is a companion view on a Tidbyt LED display. It does not live in this repo. It
is at `a separate Tidbyt app directory` on the developer machine and runs
on the host beside the backend, reading `http://127.0.0.1:3000` every 300 seconds.

It renders all clear, per room severity, and an explicit NO SIGNAL when the backend
cannot be reached, rather than leaving a stale all clear on screen. If you change the
state names in this repo, change them there too: the mapping lives in
`tprefresher.star`.

## Repository layout
- backend/: Express server, configuration, durable state, and API
- public/: browser phone UI
- firmware/: PlatformIO ESP32 display firmware
- package.json: project metadata and start script
## API and configuration

| Method | Route | Purpose |
| --- | --- | --- |
| GET | /api/status | Return all bathroom statuses |
| POST | /api/bathrooms/:id/flag | Flag a bathroom, defaults to low |
| GET | /api/bathrooms/:id/flag | Bookmark or QR flag action |
| POST | /api/bathrooms/:id/flag/:level | Flag at a severity: low, urgent, or out |
| GET | /api/bathrooms/:id/flag/:level | Bookmark or QR flag at a severity |
| POST | /api/bathrooms/:id/confirm | Clear a bathroom flag after restocking |
| GET | /api/laundry | Wash and dry timer state |
| POST | /api/laundry/:id/start | Start a cycle, optional ?minutes= |
| POST | /api/laundry/:id/clear | Acknowledge a finished cycle, or cancel a running one |

GET /api/status carries both bathrooms and laundry, so the display fetches one
payload for both of its screens.

## Two screens

The display has a tab bar under the header: PAPER and LAUNDRY. Tabs sit there rather
than at the bottom edge because a strip in the last 36 pixels is a poor target.

Layout is driven by constants at the top of main.cpp: TAB_Y, CARD_Y, CARD_H,
CARD_GAP for the paper screen, and LCARD_Y, LCARD_H, LCARD_GAP for laundry. Move the
constants, not the arithmetic.

## Laundry timers

Two machines, wash and dry, defaulting to 35 and 60 minutes. Override with
WASH_MINUTES and DRY_MINUTES.

The timers live on the server, in backend/laundry.json, not on the display. A reboot
or a firmware push never loses a running cycle, the alert fires from the machine with
internet, and the phone or Alexa could drive them later.

Nothing stores a "done" flag. The server records when a cycle started and how long it
should run, and computes the rest, the same way bathroom escalation works.

A finished cycle emails, then repeats every LAUNDRY_REPEAT_MINUTES (default 15) up to
LAUNDRY_MAX_REMINDERS (default 3) until someone taps to clear it. Laundry sitting in
the machine is the actual problem, so a single alert missed while cooking is useless.

On the display: tap an idle machine to start it, a running one to cancel, a finished
one to clear. A cycle finishing wakes the screen even from the paper tab.

## Email alerts

Configured entirely by environment and a no-op when unset, so the app runs fine
without credentials. Set on the deployment host:

- SMTP_HOST, SMTP_PORT (default 465), SMTP_USER, SMTP_PASS
- MAIL_FROM (defaults to SMTP_USER), MAIL_TO

MAIL_TO takes several comma separated addresses. Most carriers run an email to SMS
gateway, so one of them can be a phone number and arrive as a text.

For Gmail this must be an app password, which requires 2-Step Verification. A normal
account password will not authenticate.

## Severity and escalation

Three reported levels, lowest to highest: low, urgent, out. A person reports one of
them. An unattended flag then climbs one rung every 12 hours and stops at out, so a
low flag reads urgent after 12 hours and out after 24.

Nothing stores the escalated value. The server records only when a room was flagged
and at what level, and computes the current level on every request. Confirming a
restock clears the timestamp.

Responses carry both the computed `state` and the human `reported` level. Where they
differ, the board and the phone both say so, because otherwise the board asserts
"OUT OF PAPER" when nobody said that. Keep that distinction if you touch this.

## Alexa

backend/alexa.js emulates a Philips Hue bridge on the LAN so Alexa can discover
virtual switches, four per bathroom: low, urgent, out, and restocked. Turning one on
performs that action. Turning a level switch off confirms the restock. There is no
Amazon account, no account linking, and nothing exposed to the internet.

Setup is an Alexa Routine per phrase: trigger on what you want to say, action is
turning on the matching switch. Alexa cannot answer questions about status this way.
Reading state back requires a real custom skill with a public HTTPS endpoint.

Echo devices only look for a Hue bridge on port 80, which needs elevated privileges.
ALEXA_PORT overrides it for testing, but discovery will not work anywhere except 80.
Set ALEXA=off to disable the emulation entirely. If the port cannot be bound the app
logs one line and keeps serving normally.

Verified so far: SSDP discovery replies correctly to a real M-SEARCH, the Hue API
serves description, pairing, light list, and state changes, and switching a light
moves the real bathroom state. Not verified: discovery by an actual Echo, which
needs the backend running on port 80 on an always-on host. This deployment
binds port 80 as a normal user with no elevation, contrary to the usual assumption
about low ports on macOS.

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
- The backend is deployed on the host under launchd. Verified: it restarts after a
  kill -9, and bathroom state survives the restart.
- Both the board and the backend host take addresses from DHCP. The board does not
  care, because it is reachable as tprefresher.local. The backend host address is
  compiled into the firmware, so if the host moves, the board cannot find it. the host
  has no DHCP reservation yet. This is the largest remaining fragility.

## Owner preferences

- Keep solutions practical, free, and self-hostable.
- Avoid AI-isms and em dashes in documentation.
- Never commit secrets, including firmware/include/secrets.h, Wi-Fi credentials, or API tokens.

## Good next tasks

1. Give the host a DHCP reservation at 192.168.1.x, or the compiled-in API_BASE_URL
   goes stale and the board cannot find the server.
2. Make the server address settable on the board instead of compiled in, so moving
   the backend does not require a firmware push.
3. Set up GitHub credentials on the host so the deployment can be updated with a pull
   rather than a file copy.
4. Set SMTP credentials on the host so laundry alerts actually send. Until then the
   server logs what it would have sent.
5. Replace the default 35 and 60 minute cycle times with the real ones.
6. Rename the bathroom rooms and display labels if household names differ.
7. Add Home Assistant integration. GET /api/status works as a REST sensor with no
   changes. Driving flags from HA is cleaner over MQTT.
8. Print QR codes for the GET flag routes and put them in the bathrooms.
9. Enable and document API_TOKEN for deployments beyond a trusted LAN.

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
- Touch X is NOT inverted, even though the display is rotated 180 degrees and Y is.
  The rotation cancels an inversion the panel already had. This was wrong for a long
  time without anyone noticing, because every control spanned the full width of the
  screen, so a mirrored X still landed on the right card. The tab bar was the first
  left/right split and exposed it immediately. If you add anything else side by side,
  verify horizontal touch explicitly rather than assuming it works.
- The panel reaches nearly its full assumed range, x 444..3622 and y 321..3829
  against assumed 280..3860 and 340..3860. Unreachable edges and bad calibration are
  both dead ends; they were investigated and ruled out.
