# Sera-vice bust v0 — chibi Sera with a face window for the LOLIN S3 Mini Pro

Marcus asked Nov 5, 2026 (10:42 AM ET, right after the board first booted at
work) whether an AI can make a 3D model: a low-poly / chibi bust of Sera with
a space left open on the face for the ESP32-S3 board. Two routes were built
in this folder, plus the AI-generator route documented below.

## The two builds in this folder

### A. OpenSCAD two-part build (lead) — `sera-bust.scad`
Parametric source; render with OpenSCAD (`part="body"` / `part="frame"`).
- `sera-bust-body.stl` — bust with a front pocket cut out.
- `sera-face-frame.stl` — snap-over frame with the screen window + 3 button
  holes; print this small part FIRST as a test-fit for the board.
- Pocket is **landscape**: 34.3 wide x 25.4 tall (board's long axis horizontal).
- `preview-front.png` / `preview-34.png` — renders of the design.
- All the uncertain numbers are flagged `CALIPER-CHECK` at the top of the
  .scad — see below.

### B. Python single-piece build (alternative) — `sera-bust.py`
`python3 sera-bust.py` regenerates `sera-bust-v0-single-piece.stl`
(88 x 88 x 97.5 mm, 3,635 triangles, verified: 0 non-finite facets).
- One piece: plinth + shoulders + neck + head + hair + pearl headband + ears.
- Face window 17 x 17 mm; board slides in from a rear slot into an inner
  26 x 36 mm tube; a front stop-frame holds the screen just behind the
  window. USB-C cable trails out the rear slot (plug it in before sliding
  the board home).
- Pocket is **portrait**: 25.4 wide x 34.3 tall. The 128x128 screen is
  square, so portrait and landscape both display correctly — it is purely
  a question of which way the board sits and how tall/wide the head is.
- All dimensions are parameters at the top of the script.

## Fit numbers — verified vs measure-first

Verified (specs + this morning's test):
- Board: LOLIN S3 Mini Pro, D1-mini footprint ~34.3 x 25.4 mm, ESP32-S3FH4R2,
  4 MB flash / 2 MB PSRAM, QMI8658C IMU. Screen is the **ST7789** variant
  (the GC9A01 build showed a garbled screen; ST7789 DIO/40 boots clean).
- Screen: 0.85" TFT, 128x128 — viewable area ~15.3 x 15.3 mm square.
  Both builds cut the window larger than that (17 mm / 20 mm) for margin.

Measure with calipers on the real board before printing v1:
- Exact screen-glass size and its offset from the board edges/centre
  (both builds guess; the .scad flags SCREEN_DY as a guess).
- Button positions (the frame's 3 holes are placed from a guess: BTN_DX,
  BTN_DZ flagged in the .scad). The board's buttons are on its edge — check
  whether they are even reachable once the board is seated.
- Which edge USB-C is on, and how the cable will exit.
- IR receiver: the pending ~$7.49 VS1838B on GPIO14 is NOT accounted for
  in either v0 — it will need a small hole/notch wherever it ends up.
  Do not treat either v0 as final until the receiver placement is decided.

## The AI-generator route (likeness, not fit)

Yes — an AI can make the model, but it cannot make the *fitting* part.
Text/image-to-3D tools return a mesh with no exact dimensions, so the face
cavity still has to be cut afterwards in Blender/OpenSCAD. Use AI for
Sera's likeness, CAD for the board pocket.

Free tiers only — no paid plan needed or recommended for this:
- **Meshy** — image-to-3D or text-to-3D, exports STL/3MF direct.
  Free: 100 credits/month. Free outputs are CC BY 4.0 (credit Meshy).
- **Tripo AI** — image-to-3D or text-to-3D, exports STL/3MF, has an
  auto-repair pass. Free: 200 credits/month (~13 models), 15 exports;
  free models are public, non-commercial.
- **No-signup demos**: Hi3DGen and the Tencent Hunyuan3D Hugging Face demo
  both export STL without an account.
- Best input image (already on file, front-facing, plain background):
  `~/workspace/your_files/sera-scrapbook/2026-09-21-avatar-chibi.webp`
  — brown hair, white pearl headband, green top. That is the chibi look
  Marcus picked Sep 22 from the "green shirt lady with the headband"
  reference. (Current live avatar is the Nov 2 16-bit retro cozy witch;
  use the chibi file if the bust should match the chibi look.)
- Text prompt starting point, if generating from words instead:
  "low-poly chibi anime girl bust, long brown hair, white pearl headband,
  green top, big head, cute figurine for 3D printing, front-facing"
- Honest caveat from the 2026 roundups: almost no AI output slices
  cleanly as-is — expect a manifold / wall-thickness repair pass
  (Tripo's auto-repair, or Meshy + a slicer check) before printing.

## Print notes
- Both builds are low-poly on purpose — flat facets are the aesthetic, and
  they print fast. Supports needed under the chin and the hair underside
  (tree supports); the two-part build's frame prints flat.
- Test-fit order: frame (or a pocket-only test) -> full bust. Do not print
  the whole bust first and discover the pocket is 1 mm off.
- Printer model unknown to Sera — both STLs are printer-agnostic; slice
  for whatever Marcus has.

## Cross-reference & status (2026-11-05)

**STATUS: ON HOLD** (Marcus, Nov 5) — do not print yet; fit numbers unverified.

This bust was designed against the Sera-vice **as it exists on Nov 5, 2026**:
- Board: LOLIN S3 Mini Pro V1.0.0, **ST7789 panel variant** (confirmed by
  elimination — the GC9A01 firmware build displayed garbled).
- Firmware: Sera-vice v1, `../firmware-s3minipro/` — working flash file is
  `sera-vice-st7789-dio40-full.bin` (DIO / 40 MHz headers are MANDATORY on
  this rev v0.2 chip; QIO/80 boot-loops in the ROM loader).
- Orientation contract: the v1 firmware draws **landscape** with the board's
  long axis horizontal — build A's pocket matches that as-is. Build B
  (portrait) still displays fine (square screen) but the mood faces would
  render rotated unless the firmware's TFT rotation changes.
- Buttons on GPIO0/47/48 are front-face; build A's frame holes assume the
  positions flagged CALIPER-CHECK in `sera-bust.scad`.
- GPIO14 is reserved in the firmware for a future VS1838B IR receiver —
  neither bust build blocks the header edge where it would mount.
- If the board ever runs the official Muse Gadget firmware instead (see
  `../muse-gadget/`), its avatar UI targets the same screen; the bust and
  its face window carry over unchanged.
