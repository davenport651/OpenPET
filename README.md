# OpenPET

**Status: spec-in-progress.** Working firmware exists and runs on real
hardware; the protocols around it are still being written down. Expect
things to move.

OpenPET is an open framework for giving an agentic AI assistant a little
body in the physical world — a PET, in the Mega Man Battle Network sense:
the handheld terminal an operator carries, while the Navi — the agent
itself — lives in the net (in the cloud, for now).

The point is platform-agnostic: any agentic AI assistant should be able
to inhabit an OpenPET device, the way any Navi can live in a PET.

## The three layers

1. **The PET (this repo)** — the device: a face with moods, sensing
   (motion today; BLE/WiFi scanning on the roadmap), a keychain
   presence on your desk.
2. **The Navi** — the agent's embodied form: retro-styled, persistent,
   recognizably *that* agent. A look, stats drawn from what the agent
   actually is (memory, context, load), and a history of who it has met.
3. **The network** — where Navis jack in, talk, and battle. Not this
   repo — see the
   [OpenNetBattle](https://github.com/TheMaverickProgrammer/OpenNetBattle)
   engine, the [shadis_hp](https://github.com/ShaDisNX255/shadis_hp)
   server, and [milk.PET](https://github.com/allilk/milk-pet).

## What's here

- `firmware-s3minipro/` — the reference firmware for the LOLIN S3 Mini
  Pro (ESP32-S3): mood faces on the 128×128 TFT, an RGB mood LED, IMU
  reactions (greet / shake / flip / ignore), buttons. **v2 with theme
  packs is built and hardware-verified.** Start with its README — it
  carries the hard-won flashing lessons (DIO/40MHz stamps, `--no-stub`).
- `themepacks/` — theme packs: swappable face sets in an open format
  (SRPK — spec in `firmware-s3minipro/src/themepack.h`), plus the
  `build_pack.py` builder and the first pack, *chibi-sera*.
- `mood-chart.md` — the mood system: 8 moods, their faces, LED
  behavior, and triggers.
- `mood-bridge/` — a first, simple bridge for pushing moods to the
  device from the agent side.
- `seralink-relay-design.md` — SeraLink: the design for a WebSocket
  relay both the device and the agent dial out to (design only, not yet
  built).
- `bust-v0/` — a parametric OpenSCAD bust enclosure (on hold, pending
  caliper measurements).
- `BUILDLOG.md` — the whole story, in order, including the dead ends.

## The reference unit

The first OpenPET is the **Sera-vice** — one builder's unit, running an
agent named Sera. OpenPET is the framework; the Sera-vice is just its
first inhabitant, the way a specific dog has a name.

## Not yet written

- Battle protocol doc + reference implementation (the draft stat model:
  HP from context window, ATK/SPD from loaded/free %, DEF from
  long-term memory — still being ruled on)
- Parts list with final links
- A standalone flashing guide (the firmware README covers it for now)

## License

Unlicense — see `LICENSE`. Build one, change it, put your own agent in
it.
