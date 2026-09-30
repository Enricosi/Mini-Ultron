#!/usr/bin/env python3
"""
Convert a GIF or a video (MP4, WEBM, MOV...) into a Mini-Ultron 1-bit face
animation (.ula), plus an optional PROGMEM C header with the same bytes.

The .ula format (little-endian) is shared by both storage paths, so the
firmware has exactly one parser (see firmware/esp32/hello_ultron/anim_format.h):

    offset  size        field
    0       4           magic "ULA1"
    4       2           width  (px)
    6       2           height (px)
    8       2           frame count N
    10      2           reserved (0)
    12      2           color RGB565 the "on" pixels are drawn with
    14      2           reserved (0)
    16      2*N         per-frame delay in ms
    16+2N   N*H*ceil(W/8)  frames, row-major, MSB = leftmost pixel, 1 = on

Examples:
    python anim_convert.py laugh.gif --name evil_laugh --preview
    python anim_convert.py clip.mp4 --fps 12 --fit cover --dither --max-frames 60
    # built into the firmware (PROGMEM):
    python anim_convert.py laugh.gif --format h \
        --out-dir firmware/esp32/hello_ultron/src/animations
    # stored on the ESP32's LittleFS:
    python anim_convert.py laugh.gif --format ula \
        --out-dir firmware/esp32/hello_ultron/data/anim

Needs Pillow (pip install -r tools/anim/requirements.txt); videos also need
the ffmpeg/ffprobe command-line tools on PATH.
"""
import argparse
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageFilter, ImageOps, ImageSequence

MAGIC = b"ULA1"
HEADER_SIZE = 16
MIN_DELAY_MS = 20      # the SSD1331 over software SPI can't show frames much faster anyway
MAX_DELAY_MS = 65535
NAME_RE = re.compile(r"^[a-z0-9_]{1,24}$")  # must match animNameIsValid() in the firmware


def rgb888_to_565(hex_color: str) -> int:
    value = int(hex_color.lstrip("#"), 16)
    r, g, b = (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def sanitize_name(raw: str) -> str:
    name = re.sub(r"[^a-z0-9_]+", "_", raw.lower()).strip("_")[:24]
    if not NAME_RE.match(name):
        sys.exit(f"error: can't derive a valid animation name from {raw!r}; pass --name")
    return name


# --- frame sources: each yields (PIL RGB image, delay_ms) ---

def gif_frames(path: Path, fps: float | None):
    with Image.open(path) as img:
        for frame in ImageSequence.Iterator(img):
            rgba = frame.convert("RGBA")
            background = Image.new("RGBA", rgba.size, (0, 0, 0, 255))  # transparency -> off
            background.alpha_composite(rgba)
            delay = round(1000 / fps) if fps else (frame.info.get("duration") or 100)
            yield background.convert("RGB"), delay


def video_frames(path: Path, fps: float):
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height", "-of", "json", str(path)],
        capture_output=True, text=True, check=True)
    stream = json.loads(probe.stdout)["streams"][0]
    w, h = int(stream["width"]), int(stream["height"])

    proc = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-i", str(path), "-vf", f"fps={fps}",
         "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
        stdout=subprocess.PIPE)
    frame_size = w * h * 3
    delay = round(1000 / fps)
    try:
        while True:
            raw = proc.stdout.read(frame_size)
            if len(raw) < frame_size:
                break
            yield Image.frombytes("RGB", (w, h), raw), delay
    finally:
        proc.stdout.close()
        proc.wait()


# --- per-frame processing ---

def to_gray(rgb: Image.Image, args) -> Image.Image:
    if args.crop:
        w, h, x, y = args.crop
        rgb = rgb.crop((x, y, x + w, y + h))
    if not args.chroma_key:
        return rgb.convert("L")
    # Green-screen: a pixel is background when green clearly dominates. Keyed
    # background -> 0 (off); the subject keeps its brightness, or becomes a
    # solid 255 (on) with --silhouette, which reads better at 96x64.
    r, g, b = rgb.split()
    green_lead = ImageChops.subtract(g, ImageChops.lighter(r, b))  # g - max(r, b), clamped at 0
    background = ImageChops.multiply(green_lead.point(lambda v: 255 if v > 40 else 0),
                                     g.point(lambda v: 255 if v > 90 else 0))
    if args.silhouette:
        subject = Image.new("L", rgb.size, 255)
    else:
        subject = rgb.convert("L").point(lambda v: max(v, 1))
    return Image.composite(Image.new("L", rgb.size, 0), subject, background)


def fit_frame(img: Image.Image, width: int, height: int, mode: str) -> Image.Image:
    if mode == "stretch":
        return img.resize((width, height), Image.LANCZOS)
    if mode == "cover":
        return ImageOps.fit(img, (width, height), Image.LANCZOS)
    # contain: scale to fit, letterbox with black (off pixels)
    scaled = ImageOps.contain(img, (width, height), Image.LANCZOS)
    canvas = Image.new("L", (width, height), 0)
    canvas.paste(scaled, ((width - scaled.width) // 2, (height - scaled.height) // 2))
    return canvas


def to_mono(img: Image.Image, args) -> Image.Image:
    mono = _to_mono_filled(img, args)
    if not args.outline:
        return mono
    # Keep only the edge: the shape minus itself shrunk by 1 px. Done at the
    # final resolution so the line is always exactly 1 px wide on the OLED.
    filled = mono.convert("L")
    shrunk = filled.filter(ImageFilter.MinFilter(3))
    return ImageChops.subtract(filled, shrunk).convert("1", dither=Image.Dither.NONE)


def _to_mono_filled(img: Image.Image, args) -> Image.Image:
    if args.autocontrast:
        img = ImageOps.autocontrast(img)
    if args.invert:
        img = ImageOps.invert(img)
    if args.dither:
        return img.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
    return img.point(lambda p: 255 if p >= args.threshold else 0).convert("1", dither=Image.Dither.NONE)


def pack_frame(mono: Image.Image) -> bytes:
    # Pillow's "1" mode raw bytes are already row-major, MSB-first, rows padded
    # to a whole byte, 1 = white = on - exactly the .ula frame layout.
    return mono.tobytes()


def build(args) -> tuple[bytes, list[Image.Image], list[int]]:
    src = Path(args.input)
    if src.suffix.lower() == ".gif":
        source = gif_frames(src, args.fps)
    else:
        source = video_frames(src, args.fps or 12)

    frames: list[bytes] = []
    previews: list[Image.Image] = []
    delays: list[int] = []
    for rgb, delay in source:
        mono = to_mono(fit_frame(to_gray(rgb, args), args.width, args.height, args.fit), args)
        packed = pack_frame(mono)
        delay = max(MIN_DELAY_MS, int(delay))
        # Identical consecutive frames cost flash but show nothing new: merge them.
        if frames and packed == frames[-1] and delays[-1] + delay <= MAX_DELAY_MS:
            delays[-1] += delay
            continue
        if args.max_frames and len(frames) >= args.max_frames:
            break
        frames.append(packed)
        previews.append(mono)
        delays.append(min(delay, MAX_DELAY_MS))

    if not frames:
        sys.exit("error: no frames decoded from input")

    header = MAGIC + struct.pack("<HHHHHH", args.width, args.height, len(frames), 0,
                                 rgb888_to_565(args.color), 0)
    blob = header + struct.pack(f"<{len(delays)}H", *delays) + b"".join(frames)
    return blob, previews, delays


def write_header_file(path: Path, name: str, blob: bytes, source: str, w: int, h: int, n: int):
    symbol = f"ANIM_{name.upper()}"
    lines = []
    for i in range(0, len(blob), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in blob[i:i + 16]) + ",")
    path.write_text(
        f"// Generated by tools/anim/anim_convert.py from {source} - do not edit by hand.\n"
        f"// {w}x{h}, {n} frames, {len(blob)} bytes. Register it in anim_library.cpp.\n"
        f"#pragma once\n#include <Arduino.h>\n\n"
        f"static const uint8_t {symbol}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};\n")


def write_preview(path: Path, previews: list[Image.Image], delays: list[int], color: str):
    value = int(color.lstrip("#"), 16)
    rgb = ((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF)
    scale = 4
    out = []
    for mono in previews:
        colored = ImageOps.colorize(mono.convert("L"), black=(0, 0, 0), white=rgb)
        out.append(colored.resize((mono.width * scale, mono.height * scale), Image.NEAREST))
    out[0].save(path, save_all=True, append_images=out[1:], duration=delays, loop=0)


def main():
    p = argparse.ArgumentParser(description="GIF/video -> Mini-Ultron 1-bit face animation (.ula / PROGMEM .h)")
    p.add_argument("input", help="source .gif, .mp4, .webm, .mov, ...")
    p.add_argument("--name", help="animation name, [a-z0-9_], max 24 chars (default: from file name)")
    p.add_argument("--out-dir", default="anim_out", help="output directory (default: ./anim_out)")
    p.add_argument("--format", choices=["both", "ula", "h"], default="both",
                   help="ula = LittleFS file, h = PROGMEM header (default: both)")
    p.add_argument("--width", type=int, default=96, help="target width (default 96, the SSD1331)")
    p.add_argument("--height", type=int, default=64, help="target height (default 64)")
    p.add_argument("--fit", choices=["contain", "cover", "stretch"], default="contain",
                   help="contain = letterbox, cover = crop to fill, stretch = distort (default contain)")
    p.add_argument("--fps", type=float, help="sampling rate for videos (default 12); for GIFs, overrides their own timing")
    p.add_argument("--max-frames", type=int, help="stop after this many (distinct) frames")
    p.add_argument("--threshold", type=int, default=128, help="0-255 cut-off when not dithering (default 128)")
    p.add_argument("--dither", action="store_true", help="Floyd-Steinberg dithering instead of a hard threshold")
    p.add_argument("--autocontrast", action="store_true", help="stretch each frame's contrast first (helps dim videos)")
    p.add_argument("--invert", action="store_true", help="swap on/off (for dark-on-light sources)")
    p.add_argument("--crop", help="W:H:X:Y region of the source to keep, applied first (source pixels)")
    p.add_argument("--chroma-key", action="store_true", help="green-screen source: green background -> off")
    p.add_argument("--silhouette", action="store_true", help="with --chroma-key: draw the subject as a solid shape")
    p.add_argument("--outline", action="store_true", help="draw only the 1 px outline of the shapes (best with --silhouette)")
    p.add_argument("--color", default="FFFFFF", help="RGB hex the on pixels are drawn with (default FFFFFF)")
    p.add_argument("--preview", action="store_true", help="also write a 4x-scaled <name>_preview.gif to check the result")
    args = p.parse_args()

    if not 1 <= args.width <= 96 or not 1 <= args.height <= 64:
        sys.exit("error: the current display (SSD1331) is 96x64; width/height must fit inside it")
    if args.crop:
        try:
            args.crop = tuple(int(v) for v in args.crop.split(":"))
            assert len(args.crop) == 4 and args.crop[0] > 0 and args.crop[1] > 0
        except (ValueError, AssertionError):
            sys.exit("error: --crop must be W:H:X:Y, e.g. 1080:720:0:800")
    if args.silhouette and not args.chroma_key:
        sys.exit("error: --silhouette needs --chroma-key")
    name = args.name or sanitize_name(Path(args.input).stem)
    if not NAME_RE.match(name):
        sys.exit(f"error: invalid --name {name!r}, use [a-z0-9_], max 24 chars")

    blob, previews, delays = build(args)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    written = []
    if args.format in ("both", "ula"):
        (out_dir / f"{name}.ula").write_bytes(blob)
        written.append(f"{name}.ula")
    if args.format in ("both", "h"):
        write_header_file(out_dir / f"anim_{name}.h", name, blob, Path(args.input).name,
                          args.width, args.height, len(previews))
        written.append(f"anim_{name}.h")
    if args.preview:
        write_preview(out_dir / f"{name}_preview.gif", previews, delays, args.color)
        written.append(f"{name}_preview.gif")

    total_ms = sum(delays)
    print(f"{name}: {args.width}x{args.height}, {len(previews)} frames, "
          f"{total_ms / 1000:.2f} s, {len(blob)} bytes -> {out_dir}/ ({', '.join(written)})")


if __name__ == "__main__":
    main()
