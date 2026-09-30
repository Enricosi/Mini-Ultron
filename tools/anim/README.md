# Face animations (GIF/MP4 → OLED)

Turns a GIF or video into a 1-bit animation for Ultron's SSD1331 face (96x64),
in the `.ula` format the firmware plays (`firmware/esp32/hello_ultron/anim_format.h`).

```
GIF/MP4 --anim_convert.py--> .ula bytes --+--> anim_<name>.h  (PROGMEM, compiled in)
                                          +--> <name>.ula     (LittleFS file)
ESP32: anim_player streams ONE frame (768 B) at a time -> face.cpp draws only changed rows
```

## Setup

```bash
ultron_env/bin/pip install -r tools/anim/requirements.txt   # Pillow
# videos also need ffmpeg + ffprobe on PATH
```

## Convert

```bash
ultron_env/bin/python tools/anim/anim_convert.py laugh.gif --name evil_laugh --color F80000 --preview
```

Useful options (`--help` for all):

| Option | What it does |
|---|---|
| `--fit contain\|cover\|stretch` | letterbox (default) / crop to fill / distort |
| `--dither` | Floyd-Steinberg instead of a hard threshold — better for photos/video |
| `--threshold N` | 0-255 cut-off when not dithering (default 128) |
| `--autocontrast`, `--invert` | fix dim or dark-on-light sources |
| `--fps N` | video sampling rate (default 12); for a GIF, overrides its own timing |
| `--max-frames N` | cap length (each frame is 768 B) |
| `--crop W:H:X:Y` | keep only this region of the source first (e.g. a vertical video's action band) |
| `--chroma-key` | green-screen source: the green background becomes "off" |
| `--silhouette` | with `--chroma-key`: subject drawn as a solid shape (reads best at 96x64) |
| `--outline` | only the 1 px edge of the shapes (touching subjects share one outline) |
| `--color RRGGBB` | color of the "on" pixels on the SSD1331 |
| `--preview` | writes a 4x `<name>_preview.gif` to check the result before flashing |

Identical consecutive frames are merged automatically (their delays add up).

## Option A — built in (PROGMEM), for a few small animations

```bash
ultron_env/bin/python tools/anim/anim_convert.py laugh.gif --name evil_laugh --format h \
    --out-dir firmware/esp32/hello_ultron/src/animations
```

Then add two lines to `anim_library.cpp` (the `#include` and a table entry) and
compile/upload as usual. Costs app flash — which is already ~86% — so keep this
for a handful of short ones.

## Option B — LittleFS, for many animations

1. Build with the filesystem enabled: `MINI_ULTRON_ENABLE_ANIM_FS 1` in
   `features.h` (or `--build-property "compiler.cpp.extra_flags=-DMINI_ULTRON_ENABLE_ANIM_FS=1"`).
   It uses the existing 1.375 MB `spiffs` partition of the default table
   (offset `0x290000`, size `0x160000`) — no partition change.
2. Convert into `data/anim/`:
   ```bash
   ultron_env/bin/python tools/anim/anim_convert.py laugh.gif --name evil_laugh --format ula \
       --out-dir firmware/esp32/hello_ultron/data/anim
   ```
3. Build the image and flash it (this **overwrites the whole partition** —
   every animation must be in `data/anim/` each time):
   ```bash
   T=~/.arduino15/packages/esp32/tools
   $T/mklittlefs/*/mklittlefs -c firmware/esp32/hello_ultron/data -p 256 -b 4096 -s 0x160000 /tmp/littlefs.bin
   $T/esptool_py/*/esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x290000 /tmp/littlefs.bin
   ```
   (Check the port with `arduino-cli board list` first.) Flashing the app does
   not erase this partition, and vice versa.

A built-in animation shadows a LittleFS file with the same name.

## Play (USB Serial debug console)

```
ANIM:LIST              list built-in + LittleFS animations
ANIM:PLAY:scan         play once, then back to the current face
ANIM:LOOP:scan         loop until ANIM:STOP or any face change
ANIM:STOP
```

Any `setFace()` (e.g. the conversation going LISTENING/THINKING/SPEAKING)
cancels a running animation — the conversation always wins.

## Limits

- Max 96x64 (smaller is centered). Names: `[a-z0-9_]`, max 24 chars.
- Min frame delay 20 ms; real speed is bounded by the software-SPI OLED. Full-screen
  changes are the slowest case; mostly-static animations are cheap because only
  changed rows are re-sent.
