# Mini-Ultron — Mechanical / 3D printing

Infrastructure for CAD, 3D-printed parts, and mechanical documentation. Nothing here
is a finished design yet — this is the folder layout the mechanical side of the
project will grow into, so files don't end up as `final_v7_REAL.stl` in a random
place.

## Source of truth

**FreeCAD `.FCStd` files in `cad/` are the source of truth.** Everything else
mechanical is derived from them:

```
FreeCAD (.FCStd, parametric)
        |
        v
export STL / 3MF  ->  exports/stl/ , exports/3mf/
        |
        v
OrcaSlicer
        |
        v
3D printer
```

`exports/stl/` and `exports/3mf/` are generated, distribution-only files — never
hand-edit an STL, regenerate it from the `.FCStd` instead.

## Folders

- `cad/` — editable, parametric FreeCAD source files (`.FCStd`). Design happens here.
- `exports/stl/` — STL exported from `cad/`, for slicing/printing. Not editable source.
- `exports/3mf/` — 3MF exported from `cad/`, when a slicer/print project needs it.
- `drawings/` — dimensioned drawings, PDFs, mechanical schematics.
- `references/` — datasheets, measurements, photos used as design input (e.g. motor/
  servo/shield dimensions from `../../freenove-car-reference/Datasheet/`).
- `prototypes/` — experimental parts, tolerance test prints, fixtures, anything not
  meant to be a final piece yet.

## Naming convention

`snake_case`, descriptive, with a revision suffix when a part has iterated:

```
camera_mount_v01.FCStd
camera_mount_v01.stl
esp32_bracket_v01.FCStd
neck_servo_mount_v02.FCStd
```

Avoid `final.stl`, `final2.stl`, `nuevo.stl`, `test123.stl` — Git already keeps
history, so don't version-suffix a file that hasn't actually changed shape; do bump
the revision number when a physical iteration changes fit/tolerance, since prints
in `prototypes/` need an unambiguous identifier to compare against.

## Documenting a part

For any part that isn't a throwaway test, note somewhere near it (a comment in the
`.FCStd`, or a line in this file or a per-folder note):

- **Material** (e.g. PLA, PETG, TPU) and why, if it matters for the part.
- **Print orientation**, if it's not obvious or affects strength/supports.
- **Revision** and what changed from the previous one.
