#!/usr/bin/env python3
"""Convert static/icons/icon-192x192.png to ESP32 RGB565 PROGMEM header."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install pillow", file=sys.stderr)
    sys.exit(1)


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert(png_path: Path, out_path: Path, size: int) -> None:
    img = Image.open(png_path).convert("RGBA")
    img = img.resize((size, size), Image.Resampling.LANCZOS)

    pixels: list[int] = []
    for y in range(size):
        for x in range(size):
            r, g, b, a = img.getpixel((x, y))
            if a < 255:
                # Premultiply partial transparency onto white (matches splash background).
                alpha = a / 255.0
                bg = 255
                r = int(r * alpha + bg * (1.0 - alpha))
                g = int(g * alpha + bg * (1.0 - alpha))
                b = int(b * alpha + bg * (1.0 - alpha))
            pixels.append(rgb888_to_rgb565(r, g, b))

    var = "logo_solar_monitoring"
    lines = [
        "#pragma once",
        "#include <pgmspace.h>",
        "#include <stdint.h>",
        "",
        f"#define LOGO_SOLAR_MONITORING_W {size}",
        f"#define LOGO_SOLAR_MONITORING_H {size}",
        "",
        f"static const uint16_t {var}[] PROGMEM = {{",
    ]
    for i in range(0, len(pixels), 12):
        chunk = pixels[i : i + 12]
        lines.append("  " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")
    lines.append("};")
    lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {out_path} ({size}x{size}, {len(pixels) * 2} bytes)")


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    default_png = root / "static" / "icons" / "icon-192x192.png"
    default_out = Path(__file__).resolve().parents[1] / "include" / "logo_solar_monitoring.h"

    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--png", type=Path, default=default_png)
    p.add_argument("--out", type=Path, default=default_out)
    p.add_argument("--size", type=int, default=96)
    args = p.parse_args()

    if not args.png.is_file():
        print(f"Missing PNG: {args.png}", file=sys.stderr)
        sys.exit(1)
    convert(args.png, args.out, args.size)


if __name__ == "__main__":
    main()
