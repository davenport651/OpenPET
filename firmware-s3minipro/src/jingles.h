// =====================================================================
//  jingles.h - Sera's RTTTL jingles, straight from the mood chart
// ---------------------------------------------------------------------
//  IMPORTANT HARDWARE NOTE: the LOLIN S3 Mini Pro has NO buzzer and
//  no speaker, so v1 ships SILENT on purpose (see mood-chart.md -
//  "a quiet device is a polite device" - for now, a silent one too).
//
//  The RTTTL strings below are the real chart data, and playJingle()
//  is a real RTTTL player - but it only makes noise when the firmware
//  is built with a buzzer defined. To enable sound later:
//    1. wire a piezo buzzer to a free GPIO (say GPIO13),
//    2. add  -DBUZZER_PIN=13  to build_flags in platformio.ini,
//    3. rebuild. No code changes needed.
//  Without BUZZER_PIN, playJingle() just logs the tune to Serial so
//  you can watch the mood engine "sing" in the monitor.
// =====================================================================
#pragma once

#include <Arduino.h>

// --- The chart's jingles, verbatim from mood-chart.md ---------------
namespace Jingles {
static const char GREETING[]     = "Greeting:d=8,o=5,b=140:c,e,g,4c6";
static const char DONE[]         = "Done:d=8,o=5,b=150:g,4c6";
static const char EXCITED[]      = "Excited:d=16,o=5,b=160:c,e,g,c6,8g,4c6";
static const char SMUG[]         = "Smug:d=8,o=5,b=140:g,e,a,g,e";
static const char ALERT[]        = "Alert:d=8,o=5,b=120:e,p,e";
static const char DISAPPOINTED[] = "Disappointed:d=4,o=4,b=90:a#,a,g#,2g";
static const char LULLABY[]      = "Lullaby:d=4,o=5,b=80:e,d,2c";
}  // namespace Jingles

#ifdef BUZZER_PIN
// =====================================================================
//  Minimal RTTTL player (compiled only when a buzzer exists).
//  Parses "Name:d=8,o=5,b=140:notes..." - durations, sharps, dots,
//  per-note octaves and rests ('p') are all supported.
// =====================================================================
static float _rtttlNoteFreq(char note, bool sharp, int octave) {
  int semitone;  // semitones above C within the octave
  switch (note) {
    case 'c': semitone = 0; break;
    case 'd': semitone = 2; break;
    case 'e': semitone = 4; break;
    case 'f': semitone = 5; break;
    case 'g': semitone = 7; break;
    case 'a': semitone = 9; break;
    case 'b': semitone = 11; break;
    default:  return 0.0f;
  }
  if (sharp) semitone++;
  int midi = (octave + 1) * 12 + semitone;
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

inline void playJingle(const char* rtttl) {
  // Split "Name:settings:notes"
  const char* settings = strchr(rtttl, ':');
  const char* notes = settings ? strchr(settings + 1, ':') : nullptr;
  if (!settings || !notes) return;
  Serial.printf("[jingle] %.*s\n", (int)(settings - rtttl), rtttl);

  int defDuration = 4, defOctave = 6, bpm = 120;
  for (const char* p = settings + 1; p < notes; p++) {
    if (*p == 'd' && *(p + 1) == '=') defDuration = atoi(p + 2);
    if (*p == 'o' && *(p + 1) == '=') defOctave = atoi(p + 2);
    if (*p == 'b' && *(p + 1) == '=') bpm = atoi(p + 2);
  }
  const long wholeNoteMs = (60000L / bpm) * 4;  // duration of a whole note

  const char* p = notes + 1;
  while (*p) {
    int duration = defDuration;
    if (isdigit((uint8_t)*p)) {  // leading duration digits
      duration = atoi(p);
      while (isdigit((uint8_t)*p)) p++;
    }
    char note = *p ? *p++ : 'p';
    bool sharp = false;
    if (*p == '#') { sharp = true; p++; }
    int octave = defOctave;
    if (isdigit((uint8_t)*p)) octave = *p++ - '0';
    bool dotted = false;
    if (*p == '.') { dotted = true; p++; }

    long noteMs = wholeNoteMs / duration;
    if (dotted) noteMs += noteMs / 2;

    if (note == 'p') {
      delay(noteMs);
    } else {
      float f = _rtttlNoteFreq(note, sharp, octave);
      if (f > 0) tone(BUZZER_PIN, (unsigned)f, (unsigned)(noteMs * 0.9));
      delay(noteMs);
    }
    if (*p == ',') p++;
  }
  noTone(BUZZER_PIN);
}

#else  // no buzzer on this board (yet) - log instead of playing
inline void playJingle(const char* rtttl) {
  const char* colon = strchr(rtttl, ':');
  Serial.printf("[silent] jingle would play: %.*s\n",
                colon ? (int)(colon - rtttl) : (int)strlen(rtttl), rtttl);
}
#endif
