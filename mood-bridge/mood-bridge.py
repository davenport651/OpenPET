#!/usr/bin/env python3
"""mood-bridge.py -- Sera-vice mood-push bridge (v1).

Reads a mood mailbox (a local mood.json file, or the same JSON over
http(s)) and types the matching single-letter mood command into the
Sera-vice's USB serial port. Lets Sera push a mood to the device with
no firmware change: v2 already speaks these letters over 115200 serial.

WHY A BRIDGE, NOT A SINGLE LETTER
---------------------------------
v2's serial letters have FIXED, SHORT holds (from main.cpp):
    c Content sticky | h Happy 3.5s | t Thinking 5s | e Excited 3s
    s Smug 4s | w Worried 6s | g Grumpy 4s | z Sleepy sticky
A single 't' lasts five seconds, not ten minutes. So the bridge
RE-SENDS the letter at half the hold interval for as long as the
mailbox entry is live (its "until" timestamp). Re-sends are silent:
the board has no buzzer, so playJingle() only logs to serial.

HANDS ALWAYS WIN
----------------
The bridge also READS the serial stream. The firmware prints a
"[mood] <Name>" line on every mood change, including button presses,
shakes, flips, and pickups. If such a line arrives and the bridge did
not just send a letter, a human just touched her: the bridge stops
pushing for 10 minutes (or until the mailbox entry changes, whichever
comes first). A push never fights Marcus's hands.

MAILBOX SCHEMA (mood.json)
--------------------------
{
  "version": 1,
  "mood": "thinking",            // name, or "none" / "content" = no push
  "letter": "t",                 // optional: overrides the mood->letter map
  "until": "2026-11-06T16:00:00-05:00",  // ISO 8601; push ends here
  "hold_minutes": 10,            // optional instead of "until":
                                 //   until = mailbox time + hold_minutes
  "id": "20261106-1542-thinking-build",
  "set_by": "sera",
  "note": "Mid-build on the mood bridge."
}
- "until" wins over "hold_minutes" when both are present.
- A mailbox with no "until" and no "hold_minutes" is treated as an
  idle mailbox (no push). "content"/"none"/missing mood = no push;
  the bridge never sends 'c' on expiry -- she falls back to her own
  rest mood (Content, or Sleepy after 3 idle minutes) by herself.
- For a URL mailbox, "hold_minutes" counts from fetch time; for a
  file mailbox, from the file's mtime.

SCHEDULE FILE (optional, --schedule)
------------------------------------
A local desk routine the bridge runs ONLY when no mailbox push is
active and no hands-on event is recent. Same JSON shape per entry:
  {"start": "07:00", "end": "07:10", "mood": "happy",
   "days": [1,2,3,4,5], "note": "Morning greeting"}
Times are the VM's local time; days are Mon=1..Sun=7. See the
example schedule.json next to this script.

RUNNING IT
----------
  python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json
  python3 mood-bridge.py --port /dev/ttyACM0 --mailbox https://.../mood.json
  python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json \
      --schedule ./schedule.json
  python3 mood-bridge.py --mailbox ./mood.json --dry-run   # no port needed
  python3 mood-bridge.py --port /dev/ttyACM0 --mailbox ./mood.json --once
      # send the current push once and exit (smoke test)

Stop the bridge before uploading a theme pack: the bridge holds the
serial port open, so `cat pack.bin > /dev/ttyACM0` cannot use it at
the same time.

Standard library only. No pip, no pyserial: the port is opened with
os.open and configured with termios (115200, 8N1, raw, CLOCAL).
"""

import argparse
import json
import os
import select
import sys
import termios
import time
import urllib.request
from datetime import datetime, timedelta, timezone

# Firmware v2 serial letters: mood name -> (letter, hold seconds, sticky)
# Holds are from onSerialCommand() in firmware-s3minipro/src/main.cpp.
MOODS = {
    "content":  ("c", 0, True),
    "happy":    ("h", 3.5, False),
    "thinking": ("t", 5.0, False),
    "excited":  ("e", 3.0, False),
    "smug":     ("s", 4.0, False),
    "worried":  ("w", 6.0, False),
    "grumpy":   ("g", 4.0, False),
    "sleepy":   ("z", 0, True),
}

PAUSE_AFTER_HANDS_ON = 600.0   # seconds to yield after a hands-on event
PORT_RETRY_DELAY = 10.0        # seconds between serial open retries
MAILBOX_POLL = 2.0             # seconds between mailbox checks
URL_TIMEOUT = 5.0              # seconds for an http(s) mailbox fetch


def log(msg):
    print(f"[{datetime.now().astimezone().strftime('%H:%M:%S')}] {msg}",
          flush=True)


def parse_time(value):
    """Parse an ISO 8601 timestamp; naive values are taken as local time."""
    dt = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if dt.tzinfo is None:
        dt = dt.astimezone()
    return dt


class Mailbox:
    """A mood.json, read from a local path or an http(s) URL."""

    def __init__(self, source):
        self.source = source
        self.is_url = source.startswith(("http://", "https://"))
        self._stamp = None  # (mtime, size) for files; fetch time for URLs

    def read(self):
        """Return (entry dict, source_time datetime) or (None, now)."""
        now = datetime.now().astimezone()
        try:
            if self.is_url:
                with urllib.request.urlopen(self.source,
                                           timeout=URL_TIMEOUT) as resp:
                    raw = resp.read().decode("utf-8")
                stamp = now
            else:
                st = os.stat(self.source)
                stamp = (st.st_mtime, st.st_size)
                if stamp == self._stamp:
                    return None, now  # unchanged; caller keeps last entry
                self._stamp = stamp
                with open(self.source, "r", encoding="utf-8") as fh:
                    raw = fh.read()
            entry = json.loads(raw)
            if not isinstance(entry, dict):
                raise ValueError("mailbox JSON is not an object")
            return entry, (stamp if not self.is_url else now)
        except FileNotFoundError:
            log(f"mailbox not found: {self.source} (waiting)")
            return None, now
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            log(f"mailbox unreadable ({exc}); keeping last entry")
            return None, now


def resolve_push(entry, base_time):
    """Turn a mailbox entry into (letter, hold_s, until, id) or None.

    base_time is the file mtime (local mailbox) or fetch time (URL),
    used only when the entry gives hold_minutes instead of until.
    """
    if not entry:
        return None
    mood = str(entry.get("mood", "")).strip().lower()
    if mood in ("", "none", "content"):
        return None
    letter = entry.get("letter")
    if letter:
        letter = str(letter).strip().lower()
        spec = next((v for v in MOODS.values() if v[0] == letter), None)
        if spec is None:
            log(f"mailbox: unknown letter {letter!r}; ignoring entry")
            return None
        _, hold_s, _sticky = spec
    else:
        spec = MOODS.get(mood)
        if spec is None:
            log(f"mailbox: unknown mood {mood!r}; ignoring entry")
            return None
        letter, hold_s, _sticky = spec
    until = entry.get("until")
    if until:
        try:
            until_dt = parse_time(str(until))
        except ValueError:
            log(f"mailbox: bad until {until!r}; ignoring entry")
            return None
    elif entry.get("hold_minutes") is not None:
        try:
            mins = float(entry["hold_minutes"])
        except (TypeError, ValueError):
            log("mailbox: bad hold_minutes; ignoring entry")
            return None
        base = base_time
        if isinstance(base, tuple):  # file (mtime, size)
            base = datetime.fromtimestamp(base[0]).astimezone()
        until_dt = base + timedelta(minutes=mins)
    else:
        return None  # no expiry given: idle mailbox, no push
    return letter, hold_s, until_dt, str(entry.get("id", "") or "")


def open_port(path):
    """Open the serial port once and hold it; returns fd or None."""
    try:
        fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    except OSError as exc:
        log(f"cannot open {path}: {exc}")
        return None
    attrs = termios.tcgetattr(fd)
    # 115200 8N1 raw, CLOCAL|CREAD; clear HUPCL so closing later
    # does not drop DTR and reboot her.
    attrs[0] = termios.IGNPAR            # iflag
    attrs[1] = 0                         # oflag
    attrs[2] = termios.B115200 | termios.CS8 | termios.CREAD \
        | termios.CLOCAL                 # cflag
    attrs[3] = 0                         # lflag: raw, no echo
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_lines(fd, buf):
    """Non-blocking drain of the port; returns complete text lines."""
    lines = []
    try:
        while True:
            chunk = os.read(fd, 4096)
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                lines.append(line.decode("utf-8", "replace").strip())
    except BlockingIOError:
        pass
    except OSError as exc:
        log(f"serial read error: {exc}")
        raise
    return lines, buf


class Bridge:
    def __init__(self, args):
        self.port = args.port
        self.mailbox = Mailbox(args.mailbox)
        self.schedule_path = args.schedule
        self.fd = None
        self.buf = b""
        self.last_entry = None
        self.push = None          # (letter, hold_s, until, id)
        self.push_id = None
        self.expired_ids = set()  # push ids already logged as expired
        self.last_sent_at = 0.0
        self.last_sent_letter = None
        self.yield_until = 0.0
        self.yield_reason = ""
        self.schedule_cache = (None, None)  # (stamp, entries)

    # -- schedule ---------------------------------------------------
    def schedule_now(self, now):
        """Active schedule entry at `now`, or None. Mailbox wins over
        schedule; this is only consulted when no mailbox push is live."""
        if not self.schedule_path:
            return None
        try:
            st = os.stat(self.schedule_path)
            stamp = (st.st_mtime, st.st_size)
            if stamp != self.schedule_cache[0]:
                with open(self.schedule_path, encoding="utf-8") as fh:
                    data = json.load(fh)
                entries = data.get("entries", []) if isinstance(data, dict) \
                    else data
                self.schedule_cache = (stamp, entries)
            entries = self.schedule_cache[1] or []
        except (OSError, ValueError) as exc:
            log(f"schedule unreadable ({exc})")
            return None
        local = now.astimezone()
        hm = local.strftime("%H:%M")
        dow = local.isoweekday()  # Mon=1 .. Sun=7
        for ent in entries:
            if not isinstance(ent, dict):
                continue
            days = ent.get("days")
            if days and dow not in days:
                continue
            if ent.get("start", "00:00") <= hm < ent.get("end", "23:59"):
                mood = str(ent.get("mood", "")).strip().lower()
                spec = MOODS.get(mood)
                if spec and not spec[2]:  # transient moods only; a
                    return spec[0], spec[1], ent.get("id", "schedule"), \
                        ent.get("note", "")
                if spec and spec[2]:
                    return spec[0], spec[1], ent.get("id", "schedule"), \
                        ent.get("note", "")
        return None

    # -- serial -----------------------------------------------------
    def ensure_port(self):
        if self.fd is not None:
            return True
        self.fd = open_port(self.port)
        if self.fd is not None:
            log(f"serial open: {self.port} @115200 (held open)")
        return self.fd is not None

    def send(self, letter):
        try:
            os.write(self.fd, letter.encode("ascii"))
        except OSError as exc:
            log(f"serial write failed ({exc}); will reopen port")
            self.close_port()
            return False
        self.last_sent_at = time.monotonic()
        self.last_sent_letter = letter
        return True

    def close_port(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = None
            self.buf = b""

    def drain(self, now_mono):
        """Read device lines; True if a hands-on mood change was seen."""
        if self.fd is None:
            return False
        try:
            ready, _, _ = select.select([self.fd], [], [], 0)
            if not ready:
                return False
            lines, self.buf = read_lines(self.fd, self.buf)
        except OSError:
            log("serial read failed; will reopen port")
            self.close_port()
            return False
        hands_on = False
        for line in lines:
            if line.startswith("Sera-vice v2 booted"):
                log("device (re)booted; will re-assert the live push")
                self.last_sent_at = 0.0
            if line.startswith("[mood]"):
                # Caused by us if we sent a letter in the last 1.5s.
                if now_mono - self.last_sent_at > 1.5:
                    log(f"hands-on mood change seen: {line} -- yielding")
                    hands_on = True
                else:
                    log(f"device confirms: {line}")
            elif line.startswith("[imu]") or line.startswith("PACK "):
                log(f"device: {line}")
        return hands_on

    # -- main loop --------------------------------------------------
    def step(self):
        now = datetime.now().astimezone()
        now_mono = time.monotonic()

        entry, stamp = self.mailbox.read()
        if entry is not None:
            self.last_entry = entry
        push = resolve_push(self.last_entry, stamp)
        if push is not None and now < push[2] and \
                push[3] != self.push_id:
            log(f"mailbox: new push {push[3] or '(no id)'} "
                f"letter={push[0]!r} until {push[2].strftime('%H:%M:%S')}")
            self.push_id = push[3]
            self.yield_until = 0.0  # a new entry ends a hands-on yield
        self.push = push

        active = None  # (letter, hold_s, label)
        if push is not None and now < push[2]:
            active = (push[0], push[1], f"mailbox {push[3] or ''}".strip())
        elif push is not None and push[3] not in self.expired_ids:
            log(f"mailbox push {push[3] or '(no id)'} expired at "
                f"{push[2].strftime('%H:%M:%S')}; she falls back herself")
            self.expired_ids.add(push[3])
            self.push_id = None
            self.push = None
        if active is None:
            sched = self.schedule_now(now)
            if sched is not None:
                active = (sched[0], sched[1], f"schedule {sched[2]}")

        if self.fd is None and not self.ensure_port():
            return  # retry next poll

        if self.drain(now_mono):
            self.yield_until = now_mono + PAUSE_AFTER_HANDS_ON
            self.yield_reason = "hands-on"
            self.last_sent_at = 0.0

        if active is None:
            return
        if now_mono < self.yield_until:
            return  # hands-on yield in force
        letter, hold_s, label = active
        refresh_every = max(1.0, hold_s / 2.0) if hold_s else 0
        if hold_s == 0:
            # Sticky mood: send once per entry, then leave her alone.
            if self.last_sent_letter != letter or \
                    now_mono - self.last_sent_at > 60:
                if self.send(letter):
                    log(f"push [{label}]: {letter!r} (sticky)")
        else:
            if now_mono - self.last_sent_at >= refresh_every or \
                    self.last_sent_letter != letter:
                if self.send(letter):
                    log(f"push [{label}]: {letter!r} "
                        f"(refresh every {refresh_every:.1f}s)")

    def run(self, once=False):
        log("mood-bridge v1 starting "
            f"(mailbox={self.mailbox.source}, port={self.port})")
        while True:
            try:
                self.step()
            except KeyboardInterrupt:
                raise
            except Exception as exc:  # keep the bridge alive
                log(f"step failed ({exc!r}); continuing")
            if once:
                break
            time.sleep(MAILBOX_POLL)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0",
                    help="serial device (default /dev/ttyACM0)")
    ap.add_argument("--mailbox", required=True,
                    help="mood.json path or http(s) URL")
    ap.add_argument("--schedule", default=None,
                    help="optional desk-routine schedule.json")
    ap.add_argument("--once", action="store_true",
                    help="send the current push once, then exit")
    ap.add_argument("--dry-run", action="store_true",
                    help="parse the mailbox and print the push; no port")
    args = ap.parse_args(argv)

    if args.dry_run:
        mb = Mailbox(args.mailbox)
        entry, stamp = mb.read()
        print(json.dumps(entry, indent=2))
        print("resolved push:", resolve_push(entry, stamp))
        return 0

    bridge = Bridge(args)
    try:
        bridge.run(once=args.once)
    except KeyboardInterrupt:
        log("stopping (Ctrl-C)")
    finally:
        bridge.close_port()
    return 0


if __name__ == "__main__":
    sys.exit(main())
