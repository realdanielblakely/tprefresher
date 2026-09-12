# Closet Board Design Brief

Status: proposed. Not implemented.
Audience: Daniel + whoever touches `firmware/src/main.cpp` (Claude Code, Sobol).
Goal: make the TP Refresher look like a **professional product app** on a wall, not a vibe-coded hobby dashboard.

Related: [HOUSING.md](HOUSING.md) (shell later), `CLAUDE.md` (hardware truths).

**Tone lock:** no military ranks, no ship/navy language, no Daemon cosplay on this panel. This is a home closet board.

**Aesthetic lock:** quiet consumer software. Think polished system UI (calm dark list app), not Arduino sample sketches, not cyberpunk HUD, not novelty gadget chrome.

---

## 1. Why this pass

The board already works. Dark palette, fat accent blocks, Tidbyt-matched state colours, laundry tab, night backlight. That is competence.

What it still reads as: generic IoT dashboard. Built-in TFT fonts, title chrome ("TP REFRESHER"), rounded cards that feel improvised. Fine. Not product.

Housing is blocked on physical measure. UI is not. Softwaresoul is the point of this project. Later faces in the house will copy whatever language this board sets.

---

## 1b. What "professional app" means here

Daniel's direction: more *something* — specifically, it should look like software someone shipped, not firmware someone demoed.

**Yes**

- Clear hierarchy. Same margins everywhere. Aligned columns.
- One type system used consistently (sizes, weights, roles).
- Restrained colour: neutrals carry the layout; state colour is the only loud signal.
- Short, confident labels. No filler chrome.
- Touch targets that feel designed (full-width rows, predictable hit zones).
- Feels finished when idle, not "waiting for the next hack."

**No**

- Maker aesthetic: rainbow defaults, random font IDs, uneven gaps.
- Decor for its own sake: gradients, fake glass, drop shadows, skeuomorphism.
- Sci-fi / game HUD overlays.
- Dense settings-panel clutter.
- Novelty copy that undercuts trust.

**Reference mood (not clones to copy pixel-for-pixel):** dark mode list/status apps with boring confidence — the kind of UI you trust without noticing it. Translate that discipline to 320x480 and TFT_eSPI, not to a phone browser.

If a change makes it prettier but less trustworthy, reject it.

---

## 2. Hardware locks (do not fight)

| Constraint | Implication |
| --- | --- |
| 320 x 480 ST7796S, rotation 2 | Design for portrait. USB exits top. |
| Resistive touch | Big hit targets. No tiny icons as sole controls. Tabs stay off the extreme bottom. |
| TFT_eSPI today (not LVGL) | Prefer custom smooth fonts + bitmaps over a framework rewrite. Professional look comes from type, spacing, and colour discipline — not from a new UI framework. |
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

Professional rule: neutrals do structure; chroma does meaning. If everything is colourful, nothing is.

### Type

Add one custom smooth-font pair (TFT_eSPI smooth fonts or `.vlw`):

| Role | Use | Notes |
| --- | --- | --- |
| Display | Header title, big laundry countdown | Condensed, confident, few letters |
| UI | Room names, status words | Highly legible at arm length |
| Caption | Hints, "was …", LIVE/OFFLINE | Smaller, muted colour |

Avoid decorative scripts. Avoid using four built-in font IDs as a personality.

Professional rule: three roles max. Same size for the same job on every screen. Tab labels and status words should feel like one system.

### Spacing and structure

Treat layout like a product grid, not freehand drawing:

- One horizontal inset for all cards (already ~10px; keep consistent).
- One accent column width for all rows (`ACCENT_W`).
- One text start X for all primary labels (`TEXT_X`).
- Equal vertical gaps between sibling cards.
- Header, tabs, and content share an invisible column alignment.

If two elements that should match are off by a few pixels, fix it. That is the difference between "app" and "sketch."

### Layout

Keep the proven skeleton:

- Header band (~72px) with household label + LIVE/OFFLINE
- Tabs under header (`PAPER` / `LAUNDRY`) — rename only if clearer (`STOCK` / `CYCLES` ok; do not get cute)
- Cards with fat left accent (`ACCENT_W` ~76). Colour is the signal.

Tighten toward product UI:

- More breathing room between title and tabs if custom type needs it
- Status word weight > hint weight
- One bottom-line truth per paper card (already the rule after accent widening). Keep it.
- Prefer full-width row cards over floating "widget" islands if it reads cleaner
- Corner radius: keep modest and identical everywhere (no mix of 8 / 12 / 16)

### Iconography (optional v1.5)

Simple monochrome glyphs next to status words (roll / basket), 1-bit friendly. Icons never replace colour blocks. No emoji fonts. Icons should look like system symbols, not stickers.

### Motion

Allowed: short accent pulse or opacity blink on state change (under ~300ms), countdown tick.
Forbidden: looping animations, particle junk, screen-saver demos, anything that draws the eye when nothing changed.

Professional rule: motion confirms a change. It never entertains.

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

Tone check: if a label would look fine in a shipping consumer app, keep it. If it sounds like a hackathon demo, cut it.

---

## 6. Non-goals

- Rewriting the backend or Alexa bridge for aesthetics
- LVGL migration as the design vehicle
- Military, ship, or story-fiction framing on this panel
- Photo backgrounds, glassmorphism, gradients that waste flash and contrast
- Making the board brighter again
- Touch targets smaller than today
- Audio confirmations
- "Personality" that fights professional quiet (mascot faces, joke headers)

---

## 7. Implementation plan (phased)

### Phase A — Identity + polish pass (half day)

1. Rename header to the chosen household label (`STORES` default).
2. Retune header/tab spacing for the new word length; lock equal insets/gaps.
3. Normalize status words to the copy deck.
4. Sweep draw calls for alignment consistency (same X, same gaps, same radius).
5. Flash OTA. Live with it for a day.

Accept if: from the hall it looks like a finished app surface, not a prototype, and you know what the board *is*.

### Phase B — Type (one evening)

1. Add one smooth-font pair; map Display / UI / Caption roles.
2. Kill ad-hoc font ID salad in draw calls.
3. Re-check every `setTextColor` pair (light on dark). Night idle at 8/255.

Accept if: laundry countdown and room names feel like product typography, not default TFT.

### Phase C — Polish (optional weekend)

1. State-change pulse on accent only.
2. Optional 1-bit system-style glyphs.
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
7. **Product test:** show a photo to someone who did not build it. They should say it looks like an app, not a DIY board UI. If they say "cool Arduino project," keep iterating.

Fail any of these and revert the offending piece; do not stack more chrome on top.

---

## 9. File touch list

- `firmware/src/main.cpp` — palette constants, drawHeader/drawCard/drawMachine/drawTabs, copy strings, spacing constants
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

Someone who does not know the project glances at the closet and thinks a real product lives there, not that someone taped a phone sketch to the wall. It still restocks paper. It just looks shipped.
