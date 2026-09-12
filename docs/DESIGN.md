# Closet Board Design Brief

Status: proposed. Not implemented.
Audience: Daniel + whoever touches `firmware/src/main.cpp` (Claude Code, Sobol).
Goal: make the TP Refresher look intentional and house-native, not a vibe-coded utility app.

Related: [HOUSING.md](HOUSING.md) (shell later), `CLAUDE.md` (hardware truths).

**Tone lock:** no military ranks, no ship/navy language, no Daemon cosplay on this panel. This is a home closet board.

---

## 1. Why this pass

The board already works. Dark palette, fat accent blocks, Tidbyt-matched state colours, laundry tab, night backlight. That is competence.

What it still reads as: generic IoT dashboard. Built-in TFT fonts, title chrome ("TP REFRESHER"), card chrome that could be any status app. Fine. Not special.

Housing is blocked on physical measure. UI is not. Softwaresoul is the point of this project. Later faces in the house will copy whatever language this board sets.

---

## 2. Hardware locks (do not fight)

| Constraint | Implication |
| --- | --- |
| 320 x 480 ST7796S, rotation 2 | Design for portrait. USB exits top. |
| Resistive touch | Big hit targets. No tiny icons as sole controls. Tabs stay off the extreme bottom. |
| TFT_eSPI today (not LVGL) | Prefer custom smooth fonts + bitmaps over a framework rewrite. |
| Closet, wall power | Always-on glance. No audio. No party LEDs. |
| Night idle backlight already dim | Dark pixels do the work. Do not reintroduce large white fields. |
| Tidbyt companion nearby | Same state colours. Same mood grammar. Two devices, one household look. |

If a design needs LVGL, full color photos, or micro-interactions that fight resistive touch, it is the wrong design for v1.

---

## 3. Character

**Name on glass:** not "TP REFRESHER". Prefer a short household label:
- Primary candidate: `STORES`
- Acceptable alternates: `CLOSET`, `PAPER`
- Avoid: cute puns, app-store names, military titles, ship/fantasy names

**Voice:** calm household status board. Reports stock. Escalates without drama. Never jokes about bathrooms. Never sounds like Alexa.

**Glance contract (across the room):**
1. Colour block tells severity before you can read type.
2. Room or machine name is the next thing you read.
3. One status word (`OK` / `LOW` / `OUT` / `RUNNING` / `DONE`).
4. Hints and "was …" notes are secondary. They lose if space is tight.

**Household rule:** closet board = paper + laundry status. Tidbyt = morning glance / other attitude. Do not mash them into one personality.

---

## 4. Visual system

### Palette (keep unless contrast fails)

Already in `main.cpp`. Treat as law until night testing says otherwise:

- `COL_BG` near black
- `COL_CARD` charcoal
- `COL_HEADER` deep green-teal header (identity strip)
- `COL_TEXT` off-white
- `COL_MUTED` secondary grey
- State: `COL_OK`, `COL_LOW`, `COL_URGENT`, `COL_OUT`, `COL_RUNNING` (must stay Tidbyt-identical)

Do not invent a second palette for laundry. Same grammar, different nouns.

### Type

Add one custom smooth-font pair (TFT_eSPI smooth fonts or `.vlw`):

| Role | Use | Notes |
| --- | --- | --- |
| Display | Header title, big laundry countdown | Condensed, confident, few letters |
| UI | Room names, status words | Highly legible at arm length |
| Caption | Hints, "was …", LIVE/OFFLINE | Smaller, muted colour |

Avoid decorative scripts. Avoid using four built-in font IDs as a personality.

### Layout

Keep the proven skeleton:

- Header band (~72px) with household label + LIVE/OFFLINE
- Tabs under header (`PAPER` / `LAUNDRY`) — rename only if clearer (`STOCK` / `CYCLES` ok; do not get cute)
- Cards with fat left accent (`ACCENT_W` ~76). Colour is the signal.

Tighten:

- More breathing room between title and tabs if custom type needs it
- Status word weight > hint weight
- One bottom-line truth per paper card (already the rule after accent widening). Keep it.

### Iconography (optional v1.5)

Simple monochrome glyphs next to status words (roll / basket), 1-bit friendly. Icons never replace colour blocks. No emoji fonts.

### Motion

Allowed: short accent pulse or opacity blink on state change (under ~300ms), countdown tick.
Forbidden: looping animations, particle junk, screen-saver demos, anything that draws the eye when nothing changed.

### Sound

None. Closet stays silent. Visual only.

---

## 5. Copy deck

Prefer short labels. Match these unless something clearer wins on-device.

**Paper states**

| State | On glass |
| --- | --- |
| ok | `OK` or `STOCKED` (pick one; stay consistent with Tidbyt) |
| low | `LOW` |
| urgent | `URGENT` |
| out | `OUT` |

**Paper hints**

- ok: `Tap to flag`
- flagged: `Tap to clear` or `Tap to confirm` (pick one verb and keep it)

**Escalation note**

- `raised by time` (or keep `was LOW` style). One line max.

**Laundry**

- idle: `READY`
- running: `MM:SS` big
- done: `DONE`
- hints: `Tap to start` / `Tap to clear`

**Header badge**

- `LIVE` / `OFFLINE` (keep). Do not replace with icons alone.

Tone check: if a label would look fine on a municipal parking meter, it is probably right.

---

## 6. Non-goals

- Rewriting the backend or Alexa bridge for aesthetics
- LVGL migration as the design vehicle
- Military, ship, or story-fiction framing on this panel
- Photo backgrounds, glassmorphism, gradients that waste flash and contrast
- Making the board brighter again
- Touch targets smaller than today
- Audio confirmations

---

## 7. Implementation plan (phased)

### Phase A — Identity (half day)

1. Rename header to the chosen household label (`STORES` default).
2. Retune header/tab spacing for the new word length.
3. Normalize status words to the copy deck.
4. Flash OTA. Live with it for a day.

Accept if: from the hall, you know what the board *is*, not just what state it shows.

### Phase B — Type (one evening)

1. Add one smooth-font pair; map Display / UI / Caption roles.
2. Kill ad-hoc font ID salad in draw calls.
3. Re-check every `setTextColor` pair (light on dark). Night idle at 8/255.

Accept if: laundry countdown and room names feel intentional, not default TFT.

### Phase C — Polish (optional weekend)

1. State-change pulse on accent only.
2. Optional 1-bit glyphs.
3. Screenshot set (day + night) for CLAUDE.md / README.
4. Note any Tidbyt string mismatches and fix the companion to match, not the reverse, if closet is now the source of truth.

---

## 8. Acceptance tests (on hardware)

1. **Hallway glance:** from ~2m, severity colour readable before text.
2. **Night:** idle backlight + dark UI still readable without washing the closet.
3. **Touch:** all paper cards + both laundry cards + both tabs still reliable (resistive edges).
4. **Household match:** standing next to Tidbyt, colours agree for ok/low/urgent/out.
5. **Silence:** no new sounds, no RGB party modes.
6. **OTA:** one full cycle flash without USB.

Fail any of these and revert the offending piece; do not stack more chrome on top.

---

## 9. File touch list

- `firmware/src/main.cpp` — palette constants, drawHeader/drawCard/drawMachine/drawTabs, copy strings
- `firmware/data/` or `firmware/include/fonts/` — smooth fonts if Phase B
- `CLAUDE.md` — note the identity + link here when Phase A lands
- Tidbyt companion applet (separate) — only if copy/colour drift appears

Do not touch housing STL work in this pass.

---

## 10. Open choices (Daniel decides once)

1. Header word: `STORES` vs `CLOSET` vs `PAPER`
2. OK label: `OK` vs `STOCKED`
3. Confirm verb: `clear` vs `confirm`
4. Phase B font: pick one open license pair (suggest a condensed + a UI sans; name them in the PR)

Default if no answer: `STORES`, `OK`, `Tap to confirm`, and a single clean UI sans for Phase B.

---

## 11. Success looks like

Someone who does not know the project glances at the closet and thinks the board belongs there, not that someone taped a phone UI to the wall. It still restocks paper. It just looks like part of the house.
