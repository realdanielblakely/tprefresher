# Housing / enclosure brief

Practical enclosure for the TP Refresher wall board in a utility closet. Goal: clean look, cable management, OTA access, usable touch, and continuous wall-power heat OK.

## Hardware (this board)

- **AITRIP 4" ESP32-32E integrated board**: ESP32 + ST7796S + resistive touch on **one PCB**. Not a bare LCD that needs a separate MCU.
- USB-C power.
- Firmware uses rotation 180 so the USB cable exits the **TOP**.
- Mounted on a utility closet wall.
- After install: Wi-Fi OTA is primary; USB is rarely needed but must stay reachable.
- Touch must remain usable; venting/heat must be fine for always-on wall power.

## Measure first

Ask the owner for:

| Measurement | Notes |
| --- | --- |
| PCB outer L x W x H (mm) | Including connectors / glass stack if relevant |
| Mounting-hole spacing (mm) | Center-to-center for the 4 corner holes |
| Photo of current wall mount | Orientation, outlet location, nearby Tidbyt if any |

**Until measured**, treat sizes as approximate class dimensions from similar 4" modules:

- Rough outline class: **~60–70 mm x ~100–110 mm** (mark all CAD as **approximate**).
- Do not laser-cut or print a final shell until real L/W/H and hole spacing are confirmed.

## Ranked options

### A) Laser-cut acrylic sandwich (fastest clean look) — recommended v1

- Front bezel flush to the glass; rear plate behind the PCB.
- Standoffs through the 4 corner mounting holes.
- USB-C exit at the **top**, matching rotation 180.
- Wall attachment on the rear: keyholes or a French cleat.
- Cable: right-angle USB-C + cable clip / raceway to the outlet.
- Fast to iterate; looks intentional in a closet.

### B) Custom 3D-print two-piece shell

- Front bezel + rear shell; snap fit or magnets.
- Internal bosses for the PCB mounting holes.
- Top USB-C cutout; rear wall mounts.
- Optional rear USB cavity if power comes through the wall.
- Print in matte black / dark PLA or PETG.

**Nearby STLs as starting points only** (expect remesh / remodel for *this* integrated PCB):

- Printables: ST7796 enclosure by DJDevon3
- Cults: 4 inch display wall case

**Warning:** those designs usually target bare display modules or a different MCU layout. They will not drop onto the AITRIP outline without remodeling bosses, outer silhouette, and USB exit.

### C) Off-shelf / buy-and-assemble

- Shallow shadow box or deep picture frame with a face cutout.
- Small project box with a face cutout.
- Electrical low-voltage wall-plate style enclosure **if** dimensions fit after measuring.

Good when there is no laser or printer access.

### D) Tidbyt alignment

If a Tidbyt sits nearby in the same closet, match color / finish language (matte dark, similar bezel thickness) so the pair looks intentional rather than two unrelated gadgets.

## Power

- Wall USB-C **5V** supply rated for ESP32 + backlight (a few hundred mA plus headroom).
- Strain relief at the board and along the run.
- Avoid a dangling brick if possible: recessed outlet, behind-shelf brick, or short hidden pigtail.
- Right-angle USB-C at the board helps keep the top exit flush to the wall plane.

## Do not

- Block USB-C (rarely used after install, but needed for recovery; OTA is primary day-to-day).
- Cover or recess the touch surface so presses miss.
- Fully seal with zero vents if the board runs warm on continuous power.
- Leave the orange PCB and Dupont spaghetti visible on the wall.

## Recommended path

1. **Measure** PCB L/W/H and hole spacing; snap a wall photo.
2. **Acrylic sandwich v1 this week** (option A) for a clean mount and cable path.
3. **Iterate to a printed shell** (option B) later if a 3D printer is available and the acrylic fit is proven.

## Next ask for the owner

1. PCB outer length / width / height in mm.
2. Mounting-hole spacing in mm (center-to-center).
3. Photo of the current wall mount (and Tidbyt if nearby).
4. Shop access: 3D printer, laser cutter, or buy-and-assemble only?
