# Mood bridge — Sera-vice talks back (well, listens)

**What this is:** the Sera → device channel Marcus asked for on
2026-11-06: a small Python bridge that runs on the workstation VM
where the Sera-vice is plugged in, polls a `mood.json` mailbox, and
types the matching single-letter mood command into her serial port.
No firmware change — v2 already speaks these letters.

## One honest correction first

The first sketch of this idea said "a mood I push holds ~10
minutes." That is **not** how v2 works: her serial letters hold
**3–6 seconds** each (Happy 3.5, Thinking 5, Excited 3, Smug 4,
Worried 6, Grumpy 4; only Content and Sleepy are sticky). A single
letter is a flicker, not a hold. This bridge is the fix for that:
it **re-sends** the letter every half-hold for as long as the
mailbox entry is live, so a 10-minute push is ten minutes of
Thinking, not five seconds of it. Re-sends are silent — the board
has no buzzer, so the jingle only logs to serial.

The other half of that first sketch — "your hands always outrank
me" — is also enforced here, not assumed: the bridge reads her
serial output, and any `[mood]` line it didn't cause (button, shake,
flip, pickup) makes it stop pushing for 10 minutes, or until the
mailbox entry changes. A push never fights his hands.

## Files

| File | What it is |
|---|---|
| `mood-bridge.py` | The bridge. Stdlib only — no pip, no pyserial. |
| `mood.json` | The mailbox (sample, idle). The live one lives wherever the bridge is told to look. |
| `schedule.json` | Optional desk routine (morning greeting, late-night sleepy). Opt-in via `--schedule`. |
| `mood-push-playbook.md` | What Sera pushes, and when. Her job description, not code. |

## Run it (on the machine the board is plugged into)

```bash
# Mailbox as a local file — works today, no cloud piece needed.
python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json

# Same, plus the desk routine when no push is live:
python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json \
    --schedule ./schedule.json

# Mailbox over https — the shape that lets Sera push from anywhere
# (needs the public repo for this project first; see below):
python3 mood-bridge.py --port /dev/ttyACM0 \
    --mailbox https://raw.githubusercontent.com/<user>/<repo>/main/mood.json

# Smoke test: send the current push once and exit:
python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json --once

# Check a mailbox without touching the port:
python3 mood-bridge.py --mailbox ./mood.json --dry-run
```

Runs in the foreground; stop with Ctrl-C. For "always on," run it
under `systemd --user`, `tmux`, or `nohup` — whichever the VM
already uses. It reopens the port itself if the board is unplugged
and plugged back in.

**Before uploading a theme pack, stop the bridge** — it holds the
serial port open, so `cat pack.bin > /dev/ttyACM0` can't use it at
the same time. Restart it after.

## The mailbox contract

`mood.json` — one object:

```json
{
  "version": 1,
  "mood": "thinking",
  "until": "2026-11-06T16:00:00-05:00",
  "id": "20261106-1542-thinking-build",
  "set_by": "sera",
  "note": "Mid-build on the mood bridge."
}
```

- `mood`: `content` / `happy` / `thinking` / `excited` / `smug` /
  `worried` / `grumpy` / `sleepy` / `none`. (`content` and `none`
  both mean "no push" — the bridge sends nothing, and she falls
  back to her own rest mood herself.)
- `until`: ISO 8601, when the push ends. Or `hold_minutes` instead
  (counts from the file's mtime, or from fetch time for a URL).
  `until` wins if both are present. No expiry at all = idle mailbox.
- `letter`: optional override (e.g. `"letter": "t"`); the mood name
  is enough on its own.
- `id`: any string; changing it ends a hands-on yield early, so a
  new push always gets through.
- No new photos needed for any push — every mood reuses the
  portraits already in the theme pack (see `themepacks/theme-photo-log.md`).

## Where the mailbox lives — three stages

1. **Local file, today.** The bridge polls `./mood.json` on the VM.
   Marcus (or anyone on that machine) edits it to push a mood by
   hand. Good for testing the whole loop right now.
2. **GitHub raw URL, next.** The public repo for this project
   doesn't exist yet — creating it is Marcus's call, part of the
   publication plan in `BUILDLOG.md`. Once it does, `mood.json`
   lives in it and Sera pushes by committing to it; the bridge polls
   the raw URL. A mood word isn't private, and every push becomes a
   little public commit trail, which fits the project. No bridge
   rewrite needed — `--mailbox` already takes a URL.
3. **Wi-Fi in firmware v3, later.** She polls the mailbox herself
   every ~30s, credentials provisioned over serial at runtime like
   the gadget token was, never baked in. Untethered, works in the
   case/bust future. Bigger build; the BLE battle plan stays
   untouched either way.

## What was verified (2026-11-06, this VM)

- `python3 -m py_compile mood-bridge.py` — clean.
- Pty-harness test against a fake serial port: a `thinking` push
  (15-min `until`) emitted `t` three times in 9s (refresh interval
  2.5s, checked on the 2s poll — so ~3–4s apart on the wire); an
  injected `[mood] Happy` line the bridge didn't cause stopped the
  pushes and logged the 10-minute yield; an expired `until` sent
  nothing and logged the expiry exactly once; the idle sample
  mailbox parses to "no push" under `--dry-run`. A schedule entry
  covering the current time was also verified to push while the
  mailbox was idle.
- The hold durations in the code are copied from
  `firmware-s3minipro/src/main.cpp` `onSerialCommand()` — not from
  memory. The silent-replay claim is from `jingles.h`: no buzzer,
  `playJingle()` only logs.

## Not verified on real hardware yet

- That opening `/dev/ttyACM0` from Python doesn't reboot her (the
  `cat`-based pack upload opens the same port fine, so it should
  be — but watch for a `Sera-vice v2 booted.` line on first start;
  the bridge logs it and re-asserts the live push if it appears).
- The exact refresh cadence on the wire vs. her 3-minute idle
  sleep: bridge refreshes also count as activity
  (`onSerialCommand` calls `noteActivity()`), so a held push keeps
  her awake, as intended. When a push ends she settles herself.
