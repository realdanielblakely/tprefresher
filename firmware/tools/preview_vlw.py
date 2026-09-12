#!/usr/bin/env python3
"""Render a string from a .vlw exactly the way TFT_eSPI does, as a PNG.

This parses the file independently of make_vlw.py so a format mistake shows up
here rather than as garbled text on a panel that needs a flash to fix.
"""
import argparse
import struct
from pathlib import Path

from PIL import Image


def load(path):
    blob = Path(path).read_bytes()
    count, version, y_advance, _, ascent, descent = struct.unpack_from(">6i", blob, 0)
    glyphs, offset = {}, 24
    order = []
    for _ in range(count):
        unicode_point, height, width, advance, dy, dx, _unused = struct.unpack_from(">7i", blob, offset)
        offset += 28
        glyphs[unicode_point] = dict(h=height, w=width, adv=advance, dy=dy, dx=dx)
        order.append(unicode_point)
    for point in order:
        g = glyphs[point]
        size = g["w"] * g["h"]
        g["bits"] = blob[offset:offset + size]
        offset += size
    if offset != len(blob):
        raise SystemExit(f"size mismatch: parsed {offset}, file {len(blob)}")
    return dict(count=count, version=version, y_advance=y_advance, ascent=ascent, descent=descent, glyphs=glyphs)


def render(font, text, scale=3, fg=(235, 238, 242), bg=(10, 12, 16)):
    width = sum(font["glyphs"][ord(c)]["adv"] for c in text if ord(c) in font["glyphs"]) + 8
    height = font["ascent"] + font["descent"] + 8
    image = Image.new("RGB", (width, height), bg)
    pen = 4
    for char in text:
        g = font["glyphs"].get(ord(char))
        if not g:
            continue
        # TFT_eSPI: ys = y + maxAscent - dY, xs = x + dX
        top = 4 + font["ascent"] - g["dy"]
        left = pen + g["dx"]
        for row in range(g["h"]):
            for col in range(g["w"]):
                alpha = g["bits"][row * g["w"] + col] / 255
                if alpha <= 0:
                    continue
                blended = tuple(int(bg[i] + (fg[i] - bg[i]) * alpha) for i in range(3))
                image.putpixel((left + col, top + row), blended)
        pen += g["adv"]
    return image.resize((image.width * scale, image.height * scale), Image.NEAREST)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("vlw")
    parser.add_argument("text")
    parser.add_argument("--out", default="/tmp/vlw_preview.png")
    args = parser.parse_args()
    font = load(args.vlw)
    print(f"glyphs={font['count']} version={font['version']} yAdvance={font['y_advance']} "
          f"ascent={font['ascent']} descent={font['descent']}")
    render(font, args.text).save(args.out)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
