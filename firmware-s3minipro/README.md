# Sera-vice firmware — LOLIN S3 Mini Pro

A desk-pet Sera for the LOLIN S3 Mini Pro: her face lives on the 128×128
TFT, the RGB LED breathes her mood color, and the IMU lets her react to
being greeted, shaken, flipped, and ignored. Mood design is the chart in
[`../mood-chart.md`](../mood-chart.md); this is the engine that runs it.

**Status — v1: FLASHED AND VERIFIED WORKING on Marcus's board (Nov 5,
2026).** His board is the **ST7789** panel variant.
**v2 (theme packs): BUILT Nov 5, host-verified only — not yet flashed.**
Per Marcus's rule, this folder keeps only working outputs: the v1 image
`sera-vice-st7789-dio40-full.bin` and the v2 image
`sera-vice-v2-st7789-dio40-full.bin` — see "Flashing" below for why the
file names carry DIO/40 and why the other builds were removed.

---

## Verified board facts (no guesses — sources below)

| Thing | Fact | Source |
|---|---|---|
| MCU | ESP32-S3FH4R2, 4 MB flash, 2 MB PSRAM | WEMOS docs |
| Display | 0.85″ 128×128 TFT, **ST7789 *or* GC9A01** — LOLIN ships both! | WEMOS docs |
| Display wiring | MOSI 38, SCK 40, MISO 39, CS 35, DC 36, RST 34, BL 33, on HSPI | WEMOS fork Setup404/406 |
| IMU | QMI8658C, I2C **SDA 12 / SCL 11**, address **0x6B** (INT1 18, INT2 21, unused) | LOLIN Arduino example |
| Buttons | GPIO **0** (left), **47** (OK), **48** (right), active LOW | WEMOS docs + example |
| RGB LED | WS2812B: data **GPIO8**, power gate **GPIO7** (must be HIGH!) | WEMOS docs |
| IR LED | transmitter on **GPIO9** (no receiver on the board) | WEMOS docs |
| IR receiver (future) | **GPIO14 reserved** for a VS1838B OUT pin, 3.3 V only | Marcus's plan; GPIO13 also left spare |

Sources: the official product page
<https://www.wemos.cc/en/latest/s3/s3_mini_pro.html>, the official
schematic V1.0.0 linked there, LOLIN's own Arduino example
(`github.com/wemos/D1_mini_Examples` → `examples/06.S3_MINI_PRO`), and
LOLIN's TFT_eSPI fork (`github.com/wemos/TFT_eSPI`, `User_Setups/
Setup404_LOLIN_S3_MINI_PRO.h` / `Setup406_LOLIN_S3_MINI_PRO_ST7789.h`) —
the display build flags in `platformio.ini` are transcribed from those
two files, and the firmware links LOLIN's fork, not upstream TFT_eSPI.

Build target note: PlatformIO's espressif32 platform has no
`lolin_s3_mini_pro` board definition, so the project builds for
`lolin_s3_mini` — the same ESP32-S3FH4R2 board (4 MB QIO flash, 2 MB
QSPI PSRAM, native USB) minus the Pro's populated peripherals. Every
Pro pin is defined in `src/board_pins.h`, so nothing depends on the
variant header.

## What's new in v2 — theme packs

Everything from v1 (below) carries over unchanged. v2 changes where
the faces can come from, and how the flash is laid out.

### Partition map (partitions.csv)

| Partition | Offset | Size | Holds |
|---|---|---|---|
| nvs | 0x009000 | 20 KB | settings (unchanged home) |
| phy_init | 0x00F000 | 4 KB | radio calibration |
| factory (app) | 0x010000 | 1,984 KB (0x1F0000) | the firmware — single slot, **no OTA** |
| **themepack** | **0x200000** | **1 MB** | the installed SRPK theme pack |
| *(spare)* | 0x300000 | 1 MB | unallocated, reserved |

Firmware and packs now update independently: the v2 merged image spans
only 0x0–0x84E00, so flashing new firmware does **not** erase an
installed pack. (`erase_flash` still wipes everything — re-upload the
pack afterwards if you use it.)

### Fallback rule

At boot the firmware validates whatever sits in the themepack
partition (header + full payload CRC32). **Valid pack → its portraits
are the faces, mood for mood. Anything else → the v1 drawn faces,
exactly as before.** A pack is a skin, never a single point of
failure; with a pack active, blinking/animation ticks (which belong
to the drawn faces) pause and portraits change on mood changes only.

### SRPK pack format v1

The full spec is the comment block at the top of `src/themepack.h`;
the short version: magic `SRPK`, format version, header with frame
count + per-frame offset table + pack name, then the payload — one
full-frame 128×128 **RGB565** portrait per mood, in Mood enum order
(Content, Happy, Thinking, Excited, Smug, Worried, Grumpy, Sleepy) —
and a **CRC32 (zlib) over the payload** stored in the header.
A full 8-frame pack is 262,240 bytes (96 header + 8 × 32,768).
Build packs with [`../themepacks/build_pack.py`](../themepacks/build_pack.py);
the first pack, "chibi-sera", is `../themepacks/chibi-sera.pack.bin`.

### Uploading a pack (no reflash, no Python)

The serial port watches the raw byte stream for the SRPK magic —
normal single-letter commands keep working otherwise. On magic, the
pack is **staged in PSRAM and CRC-verified before the partition is
erased**, so a truncated or corrupt transfer changes nothing. She
prints `PACK OK <name>` on success (and immediately shows the new
face) or `PACK FAIL <reason>` on failure, old pack/faces untouched.
A sender that goes quiet for >5 s mid-pack aborts with
`PACK FAIL timeout`. No handshaking is needed from the sender:

```bash
stty -F /dev/ttyACM0 115200 raw -echo
cat chibi-sera.pack.bin > /dev/ttyACM0     # ~25 s at line rate; she pauses
                                           # a couple of seconds to write
```

Alternative, with esptool — write the pack straight to its partition:

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub write_flash 0x200000 chibi-sera.pack.bin
```

## What v1 does (all still true in v2)

- **Boot splash** — "SERA / sera-vice v1", which panel build it is,
  IMU status, and an R-G-B LED sweep (same trick as the factory demo).
- **All 8 chart moods**, faces drawn with TFT primitives (no art
  assets): Content, Happy, Thinking, Excited, Smug, Worried, Grumpy,
  Sleepy — with blinking, Sleepy's drifting Z's, and Excited twinkles.
- **LED matches the chart**: breathing green (Content), warm pulse
  (Happy), breathing blue (Thinking), quick bright pulse (Excited),
  steady teal (Smug), soft orange pulse (Worried), amber (Grumpy),
  dim slow blue (Sleepy). Global dimmer `LED_BRIGHTNESS` in main.cpp —
  a bare WS2812 at full tilt is a desk lamp.
- **Triggers** (local only):
  - Left button (GPIO0) → **Happy** greeting
  - OK / Right buttons (47/48) → cycle **Thinking → Smug → Worried → auto**
  - Shake (several sharp jerks) → **Excited**
  - Flip face-down for ~0.6 s → **Grumpy** (face-up reads accel Z ≈ −1 g
    on this board, per the factory demo; face-down ≈ +1 g)
  - Still for 3 minutes → **Sleepy**; pick her up → **Happy**
  - USB serial at 115200: `c h t e s w g z` force a mood, `i` dumps the
    IMU, `?` prints help — great for admiring faces from your chair.
- If the IMU doesn't answer at boot, she still runs — buttons and
  serial keep working, motion triggers switch off, splash says so.

## What's stubbed, and why

- **Sound**: the board has *no buzzer*. The chart's RTTTL jingles are
  embedded verbatim in `src/jingles.h` with a complete RTTTL player
  that compiles only when `BUZZER_PIN` is defined; otherwise
  `playJingle()` just logs the tune name to Serial. To add sound
  later: piezo on a free GPIO (13 is spare), `-DBUZZER_PIN=13` in
  `platformio.ini`, rebuild. v1 being silent is expected.
- **IR / digifights**: stub comment only. The board has an IR
  *transmitter* (GPIO9) but no receiver; battles wait for the VS1838B
  on the reserved GPIO14 plus the battle-protocol phase.
- **Muse pairing**: not in v1 — the gadget token is still pending.
- **Wi-Fi**: deliberately unused.

## Flashing

**The working images** — merged full-flash images (bootloader +
partitions + app) for Marcus's ST7789 board, written at offset 0x0:

- `sera-vice-st7789-dio40-full.bin` — **v1**, the hardware-verified one.
- `sera-vice-v2-st7789-dio40-full.bin` — **v2** (544,256 bytes; theme
  packs, above). Built + host-verified Nov 5; **not yet flashed**.

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub write_flash 0x0 sera-vice-v2-st7789-dio40-full.bin
```

(v2's image ends at 0x84E00, well below the themepack partition at
0x200000 — flashing firmware never disturbs an installed pack.)

Two hard-won facts are baked into that file name:

- **ST7789**: LOLIN ships this board with ST7789 *or* GC9A01 panels.
  Marcus's is ST7789 (the GC9A01 build booted but showed garbage).
- **DIO / 40 MHz**: his rev v0.2 chip will not boot from flash at
  QIO / 80 MHz — the ROM dies loading the bootloader
  (`ets_loader.c 78`, TG0 watchdog loop). DIO/40 boots every time.
  Any future image for this board must be stamped DIO/40.

Cleanup note (Nov 5, Marcus's "only keep working output" rule): the
QIO/80 merged images, the GC9A01 builds, the app-only images, and the
diagnostic DIO/80 + QIO/40 variants were removed from this folder.
They're all regenerable from the source here: `pio run` rebuilds both
panel apps (splash prints its panel), and a flashable full image is
`esptool merge-bin` over the build's bootloader.bin (0x0),
partitions.bin (0x8000), firmware.bin (0x10000) — **always with
`--flash-mode dio --flash-freq 40m --flash-size 4MB` for this board.**
The DIO/40 stamp must happen here, at merge time: setting
`board_build.flash_mode = dio` in platformio.ini instead makes the
build fail outright (the Arduino package ships no prebuilt dio_40m
bootloader for this chip — v2 build trap, found Nov 5).

If upload won't start, hold the left (GPIO0/BOOT) button while plugging
in USB-C to force download mode. Marcus's distro esptool (Debian 4.7.0)
is missing its stub-flasher files — hence `--no-stub` above, or use a
pip-venv esptool.

## Files

- `src/main.cpp` — mood engine, IMU triggers, buttons, LED, serial
- `src/faces.h` — the eight faces, drawn with TFT primitives
  (in v2 these are the built-in **fallback** faces)
- `src/themepack.h` — v2: the SRPK pack format spec, partition
  validation + rendering, and the cat-first serial pack receiver
- `src/jingles.h` — chart RTTTL data + the (currently silent) player
- `src/board_pins.h` — every pin, with its source
- `partitions.csv` — v2 flash layout (see the partition map above)
- `platformio.ini` — both panel envs; display config lives here as
  build flags transcribed from LOLIN's Setup404/406
- `../themepacks/` — pack builder (`build_pack.py`), the chibi-sera
  source art, and `chibi-sera.pack.bin` ready to upload

## Knobs worth turning

- `SLEEP_AFTER_MS` (main.cpp) — how long ignored before Sleepy.
  3 minutes is short for real life but right for testing on a desk.
- `LED_BRIGHTNESS` — 0.35 default. The case's diffuser helps too.
- Shake/flip sensitivity — `jerk > 0.5f` and `az > 0.55f` in `pollImu()`.

## Flashing WITHOUT PlatformIO — other routes

Arduino IDE cannot flash a bare .bin by itself — it only uploads sketches
it compiles. The merged `sera-vice-st7789-dio40-full.bin` (previous
section) works with any of these:

- **esptool bundled in Arduino IDE**: look under
  `Arduino15/packages/esp32/tools/esptool_py/`, or `pip install esptool`
  in a venv. Same command as above (esptool v5 spells it `write-flash`;
  v4 uses `write_flash`). Port: COMx on Windows, /dev/ttyACM0 on Linux,
  /dev/cu.usbmodem* on macOS.
- **Browser flasher** (zero installs): the Adafruit WebSerial ESPTool
  page in Chrome or Edge — connect, offset 0x0, pick the file, flash.

## Related work

- **Bust (ON HOLD, Nov 5):** `../bust-v0/` — chibi Sera bust designed against
  THIS firmware's board orientation (landscape, ST7789, DIO/40). Its README
  carries the cross-reference and fit caveats.
- **Official Muse Gadget path:** `../muse-gadget/` — exploration of Meta's
  muse-gadget-sdk firmware for this same board (Nov 5).
