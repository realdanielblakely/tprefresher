#!/usr/bin/env python3
"""Pull a screenshot from the board and save it as a PNG.

The firmware reads its own framebuffer back over SPI and streams raw RGB565, high
byte first, which is the display's own byte order rather than the ESP32's.
Usage: screenshot.py [host] [out.png]
"""
import socket
import struct
import sys

from PIL import Image

HOST = sys.argv[1] if len(sys.argv) > 1 else "tprefresher.local"
OUT = sys.argv[2] if len(sys.argv) > 2 else "/tmp/board.png"
W, H = 320, 480

with socket.create_connection((HOST, 4445), timeout=20) as sock:
    need = W * H * 2
    chunks, got = [], 0
    while got < need:
        block = sock.recv(min(65536, need - got))
        if not block:
            break
        chunks.append(block)
        got += len(block)

raw = b"".join(chunks)
print(f"received {got} of {need} bytes")
if got < need:
    raw += b"\x00" * (need - got)

image = Image.new("RGB", (W, H))
pixels = image.load()
for i in range(W * H):
    value = (raw[i * 2] << 8) | raw[i * 2 + 1]
    r = ((value >> 11) & 0x1F) * 255 // 31
    g = ((value >> 5) & 0x3F) * 255 // 63
    b = (value & 0x1F) * 255 // 31
    pixels[i % W, i // W] = (r, g, b)
image.save(OUT)
print("wrote", OUT)
