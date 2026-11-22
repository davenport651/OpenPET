# Sera-vice mood chart — draft 1 (2026-10-20)

Mood design is Sera's job (Marcus, 2026-10-20). Hardware target: 240x240 ST7789 display (face), WS2812 LED (status), piezo buzzer (jingles).

Design rules:
- Jingles play on mood *transitions*, never on loops. A quiet device is a polite device.
- Idle moods are silent. Only greetings, alerts, and victories make noise.
- Every jingle is an RTTTL string — short enough to flash in a batch or push as text later.

| Mood | Face (240x240) | LED | Trigger | Jingle |
|---|---|---|---|---|
| Content (idle) | soft smile, open eyes | slow breathing green | connected, nothing pending | — silent — |
| Happy / greeting | bright smile, happy closed eyes | warm green pulse | first connection of the day, button-press hello | Greeting |
| Thinking | eyes glanced up-side, dotted mouth | breathing blue | Sera is working on something from the device | — silent; Done chirp on completion |
| Excited | sparkle eyes, open smile | quick bright pulse | a win lands — build succeeds, good news delivered | Excited |
| Smug | half-lidded eyes, crooked smile | steady teal | caught an error, corrected a record, detective thread ends in a found thing | Smug (the nyah-nyah) |
| Worried / alert | raised brows, small "o" mouth | soft orange pulse | something needs Marcus — flagged item, failed job | Alert (gentle, two beeps) |
| Grumpy | flat mouth, angled brows | amber | low battery, long ignore, repeated connection failures | Disappointed (Marcus's test jingle is the incumbent; Sera's entry below) |
| Sleepy | closed eyes, tiny "Z" | dim blue slow pulse | night hours, long idle | Lullaby fragment on entry, then silent |

## Jingles (RTTTL)

```
Greeting:d=8,o=5,b=140:c,e,g,4c6
Done:d=8,o=5,b=150:g,4c6
Excited:d=16,o=5,b=160:c,e,g,c6,8g,4c6
Smug:d=8,o=5,b=140:g,e,a,g,e
Alert:d=8,o=5,b=120:e,p,e
Disappointed:d=4,o=4,b=90:a#,a,g#,2g
Lullaby:d=4,o=5,b=80:e,d,2c
```

Notes:
- "Disappointed" approximates the sad-trombone slide with descending semitones (RTTTL can't bend pitch).
- Smug's melody is the playground taunt (sol-mi-la-sol-mi) — playful, never mean.
- Alert is deliberately NOT the UPS beep. She's heard enough of that guy.
