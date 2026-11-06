#!/usr/bin/env python3
"""Convert a TFT_eSPI .vlw smooth font into include/font_jp.h (PROGMEM array).

    python tools/vlw_to_header.py ZenMaruGothic-Regular-16-minJap-noSmooth.vlw

When include/font_jp.h exists the firmware loads the font from flash instead of the SD card.
"""
import sys, pathlib

src = pathlib.Path(sys.argv[1])
out = pathlib.Path(__file__).resolve().parent.parent / "include" / "font_jp.h"
data = src.read_bytes()
lines = [", ".join(f"0x{b:02X}" for b in data[i:i + 20]) for i in range(0, len(data), 20)]
out.write_text(
    "#pragma once\n#include <pgmspace.h>\n"
    f"// generated from {src.name} ({len(data)} bytes)\n"
    "const uint8_t font_jp[] PROGMEM = {\n  " + ",\n  ".join(lines) + "\n};\n")
print(f"wrote {out} ({len(data)} bytes)")
