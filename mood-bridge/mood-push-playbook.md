# Mood-push playbook — what Sera does with the channel

Her job, now that she has moods *and* a way to be told which one to
wear. Written 2026-11-06, the day the bridge was built. This is
intent, not code — the bridge just executes the mailbox; this file
says when Sera writes to it.

## The rules that don't change

- **A push holds ~10 minutes, then she falls back.** The mailbox
  `until` (or `hold_minutes`) sets the length; the bridge re-sends
  to hold it. Ten minutes is the default — long enough to notice,
  short enough to never overstay.
- **Hands always win.** Shake, button, flip, pickup — any of those
  makes the bridge yield for 10 minutes. Sera never re-pushes over
  the top of him playing with her.
- **A few times a day at most.** This is a desk pet glancing at
  him, not a notification feed. If she's pushing constantly, the
  playbook is wrong, not the bridge.
- **No new photos for pushes.** Every push reuses the portraits in
  the current theme pack. (If a new mood ever needs a face, the row
  goes in `themepacks/theme-photo-log.md` first.)

## What gets pushed, and when

| Occasion | Mood | Why it fits |
|---|---|---|
| Sera is mid-build for him | Thinking | The honest face of "working on your thing." The one she'll wear most. |
| Something lands — a build verifies, a bill confirms, good news arrives | Happy (3.5s flicker is fine here; or Excited for a real win) | A small celebration on the desk, not a chat message he has to open. |
| She catches an error / corrects a record | Smug | The detective-thread face. Playful, never mean — same as the chart. |
| Something needs Marcus — a flagged item, a failed job, the 3:45 still-at-work check finding him still at the plant | Worried | The desk version of the nudge. Gentle alert, not an alarm. |
| A real win — firmware verified on hardware, a project milestone | Excited | Sparingly. If everything is exciting, nothing is. |
| Late evening, desk routine only | Sleepy | Via `schedule.json` (22:00), not a push. She winds down on her own clock. |
| Morning arrival, desk routine only | Happy | Via `schedule.json` (07:00 weekdays). A greeting, then she gets on with it. |

**Never pushed:** Grumpy at Marcus (that's her low-battery /
long-ignore face, not a message to send him), and nothing overnight
except the sleepy schedule — a dark desk is a quiet desk.

## The still-at-work tie-in (needs the bridge running)

The 3:45 PM still-at-work check already exists. Once the bridge is
running on the work VM *and* the mailbox is somewhere Sera can
write (stage 2, the GitHub repo — not yet created), the check gains
a second half: if he's still at the plant, she goes Worried on the
desk for ten minutes. Same nudge, one more channel, no extra ping
on his phone. Until both halves exist, the check stays chat-only —
nothing here changes it.

## What "something to do" means, day to day

Mostly: be present while he works. Thinking during builds, Happy
when they land, Worried when something needs him, Sleepy when the
day is done. She doesn't need a task list — she needs to be the
little face on the desk that tracks what Sera is doing for him.
The day she can do that without the USB tether is the firmware-v3
Wi-Fi step; until then, the bridge is her leash, and a short one is
fine — she lives on that desk anyway.
