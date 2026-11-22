# Theme Photo Log — Sera-vice

**What this is:** the running list of every portrait photo a theme pack
needs, and whether it exists yet. Sera maintains this file: check it
before generating anything, and add a row here FIRST whenever a new
mood, behavior, or theme is added — then generate against the row.

## Photo spec (applies to every theme)

- One PNG per mood, named `<mood>.png`, in the theme's folder
  (`themepacks/<theme-name>/`).
- Square source, 512×512 or larger (the builder center-crops and
  resizes to 128×128, RGB565).
- **Framing: torso-to-face bust** — face large and dominant so the
  expression reads at 128×128, torso visible at the bottom edge.
  (Marcus's art direction, 2026-11-05.)
- Plain, light background; character and style consistent across all
  8 portraits of a theme.
- Nothing special needed for color encoding — the builder handles the
  RGB565 byte order (fixed 2026-11-06).

## Mood slots (firmware order — never reorder)

Frame order mirrors the Mood enum in `firmware-s3minipro/src/faces.h`.
The pack format allows up to 16 frames, but the firmware currently
uses exactly these 8 (MOOD_COUNT); a 9th mood needs firmware work,
not just a photo.

| # | Mood    | File         |
|---|---------|--------------|
| 0 | Content | content.png  |
| 1 | Happy   | happy.png    |
| 2 | Thinking| thinking.png |
| 3 | Excited | excited.png  |
| 4 | Smug    | smug.png     |
| 5 | Worried | worried.png  |
| 6 | Grumpy  | grumpy.png   |
| 7 | Sleepy  | sleepy.png   |

## Theme 1: chibi-sera — COMPLETE (8/8)

Source art in `themepacks/chibi-sera/`, built from Marcus's September
2026 chibi reference (pearl headband, green top). Packed as
`chibi-sera.pack.bin`; colors hardware-verified 2026-11-06.

| Mood    | Status    | Notes |
|---------|-----------|-------|
| Content | generated | |
| Happy   | generated | |
| Thinking| generated | |
| Excited | generated | |
| Smug    | generated | |
| Worried | generated | |
| Grumpy  | generated | |
| Sleepy  | generated | |

## Theme 2: (not chosen yet) — 0/8

When a second theme is picked, clone the table above: all 8 moods
start as **needed**, and each flips to **generated** as its portrait
is made. A theme is not packable until all 8 rows say generated.

## Future needs (candidates — not approved features)

- **Pushed moods (Sera → device comms):** needs NO new photos; a push
  displays the existing portrait for that mood.
- **BLE battle stances** (only if the battle phase gets approved):
  possibly 1 extra "battle-ready" portrait per theme. Would become a
  new row here before any generation.
- **Seasonal/holiday themes:** each is a whole new theme = 8 photos,
  logged as its own section when proposed.
