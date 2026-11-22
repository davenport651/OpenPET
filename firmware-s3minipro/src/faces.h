// =====================================================================
//  faces.h - Sera's faces, drawn with TFT_eSPI graphics primitives
// ---------------------------------------------------------------------
//  The mood chart (../mood-chart.md) describes each face for a 240x240
//  screen; everything here is re-drawn for the S3 Mini Pro's 128x128
//  panel using circles, ellipses, lines and hand-plotted arcs, so all
//  eight moods work with zero art assets. Layout on the 128x128 canvas:
//     eyes:  centres (42, 54) and (86, 54)
//     mouth: around y = 84..94
//  drawFace() is called on mood changes, on blinks, and (for Sleepy /
//  Excited) on a slow animation tick.
// =====================================================================
#pragma once

#include <TFT_eSPI.h>

// The eight moods from the chart, in chart order.
enum Mood : uint8_t {
  MOOD_CONTENT = 0,  // idle: soft smile, open eyes
  MOOD_HAPPY,        // greeting: bright smile, happy closed eyes
  MOOD_THINKING,     // eyes glanced up-side, dotted mouth
  MOOD_EXCITED,      // sparkle eyes, open smile
  MOOD_SMUG,         // half-lidded eyes, crooked smile
  MOOD_WORRIED,      // raised brows, small "o" mouth
  MOOD_GRUMPY,       // angled brows, flat mouth
  MOOD_SLEEPY,       // closed eyes, tiny drifting "Z"
  MOOD_COUNT
};

inline const char* moodName(Mood m) {
  switch (m) {
    case MOOD_CONTENT: return "Content";
    case MOOD_HAPPY:   return "Happy";
    case MOOD_THINKING:return "Thinking";
    case MOOD_EXCITED: return "Excited";
    case MOOD_SMUG:    return "Smug";
    case MOOD_WORRIED: return "Worried";
    case MOOD_GRUMPY:  return "Grumpy";
    case MOOD_SLEEPY:  return "Sleepy";
    default:           return "?";
  }
}

namespace Faces {

const int EYE_LX = 42, EYE_RX = 86, EYE_Y = 54;
const int MOUTH_X = 64;

// ---- tiny drawing helpers -------------------------------------------
// Draw a smooth arc by stamping dots along it. Angles are degrees in
// screen space (0 = right, 90 = down, 180 = left, 270 = up), so a
// smile is 20..160 and a frown is 200..340.
inline void arc(TFT_eSPI& tft, int cx, int cy, int r, int a0, int a1,
              uint16_t color, int thick = 3) {
  for (int a = a0; a <= a1; a += 3) {
    float rad = a * 0.0174533f;
    int x = cx + (int)(r * cosf(rad));
    int y = cy + (int)(r * sinf(rad));
    tft.fillCircle(x, y, thick, color);
  }
}

// A 4-point sparkle (Excited eyes, and twinkles around the head).
inline void sparkle(TFT_eSPI& tft, int cx, int cy, int s, uint16_t color) {
  tft.drawLine(cx - s, cy, cx + s, cy, color);
  tft.drawLine(cx, cy - s, cx, cy + s, color);
  int d = s * 2 / 3;
  tft.drawLine(cx - d, cy - d, cx + d, cy + d, color);
  tft.drawLine(cx - d, cy + d, cx + d, cy - d, color);
  tft.fillCircle(cx, cy, 2, color);
}

// One open eye (with optional blink = drawn as a lid line instead).
inline void openEye(TFT_eSPI& tft, int cx, bool blink, uint16_t ink) {
  if (blink) {
    tft.fillRect(cx - 8, EYE_Y - 2, 17, 4, ink);
  } else {
    tft.fillCircle(cx, EYE_Y, 7, ink);
    tft.fillCircle(cx + 2, EYE_Y - 2, 2, TFT_BLACK);  // catchlight
  }
}

}  // namespace Faces

// ---- the faces --------------------------------------------------------
// blink: draw open eyes as lids (Content/Thinking/Worried/Grumpy only).
// tick:  slow animation phase (Sleepy's rising Z, Excited's twinkles).
inline void drawFace(TFT_eSPI& tft, Mood mood, bool blink = false, int tick = 0) {
  using namespace Faces;
  const uint16_t BG = TFT_BLACK;
  const uint16_t INK = TFT_WHITE;
  const uint16_t PINK = tft.color565(255, 150, 170);
  const uint16_t GOLD = tft.color565(255, 210, 80);
  const uint16_t ZBLUE = tft.color565(120, 160, 255);

  tft.fillScreen(BG);

  switch (mood) {
    case MOOD_CONTENT:
      openEye(tft, EYE_LX, blink, INK);
      openEye(tft, EYE_RX, blink, INK);
      arc(tft, MOUTH_X, 78, 15, 25, 155, INK);  // soft smile
      break;

    case MOOD_HAPPY:
      arc(tft, EYE_LX, EYE_Y + 4, 8, 195, 345, INK);  // ^ ^
      arc(tft, EYE_RX, EYE_Y + 4, 8, 195, 345, INK);
      arc(tft, MOUTH_X, 74, 19, 15, 165, INK, 4);     // big smile
      tft.fillCircle(22, 84, 4, PINK);                // cheeks
      tft.fillCircle(106, 84, 4, PINK);
      break;

    case MOOD_THINKING:
      // eyes glanced up-and-to-the-side
      if (blink) {
        openEye(tft, EYE_LX, true, INK);
        openEye(tft, EYE_RX, true, INK);
      } else {
        tft.fillCircle(EYE_LX + 3, EYE_Y - 3, 5, INK);
        tft.fillCircle(EYE_RX + 3, EYE_Y - 3, 5, INK);
      }
      tft.fillCircle(54, 94, 2, INK);  // dotted "hmm..." mouth
      tft.fillCircle(64, 94, 2, INK);
      tft.fillCircle(74, 94, 2, INK);
      break;

    case MOOD_EXCITED:
      sparkle(tft, EYE_LX, EYE_Y, 9, GOLD);
      sparkle(tft, EYE_RX, EYE_Y, 9, GOLD);
      // open smile: filled half-disc with a tongue
      tft.fillEllipse(MOUTH_X, 84, 14, 11, INK);
      tft.fillRect(MOUTH_X - 15, 72, 31, 12, BG);
      tft.fillEllipse(MOUTH_X, 91, 6, 3, PINK);
      // twinkles alternate with the tick
      if (tick % 2 == 0) {
        sparkle(tft, 16, 28, 5, GOLD);
        sparkle(tft, 112, 22, 4, GOLD);
      } else {
        sparkle(tft, 20, 18, 4, GOLD);
        sparkle(tft, 108, 34, 5, GOLD);
      }
      break;

    case MOOD_SMUG:
      // half-lidded: dot eyes with a lid bar across the top
      tft.fillCircle(EYE_LX, EYE_Y + 2, 6, INK);
      tft.fillCircle(EYE_RX, EYE_Y + 2, 6, INK);
      tft.fillRect(EYE_LX - 9, EYE_Y - 8, 19, 7, BG);
      tft.fillRect(EYE_RX - 9, EYE_Y - 8, 19, 7, BG);
      tft.fillRect(EYE_LX - 9, EYE_Y - 2, 19, 3, INK);
      tft.fillRect(EYE_RX - 9, EYE_Y - 2, 19, 3, INK);
      arc(tft, MOUTH_X + 5, 80, 13, 30, 140, INK);  // crooked smile, rides high on one side
      break;

    case MOOD_WORRIED:
      tft.drawLine(30, 36, 54, 31, INK);  // raised brows
      tft.drawLine(31, 39, 54, 34, INK);
      tft.drawLine(74, 31, 98, 36, INK);
      tft.drawLine(74, 34, 97, 39, INK);
      if (blink) {
        openEye(tft, EYE_LX, true, INK);
        openEye(tft, EYE_RX, true, INK);
      } else {
        tft.fillEllipse(EYE_LX, EYE_Y, 6, 8, INK);
        tft.fillEllipse(EYE_RX, EYE_Y, 6, 8, INK);
      }
      tft.drawCircle(MOUTH_X, 92, 5, INK);  // small "o" mouth
      tft.drawCircle(MOUTH_X, 92, 4, INK);
      break;

    case MOOD_GRUMPY:
      tft.drawLine(30, 34, 54, 43, INK);  // brows angled down to the middle
      tft.drawLine(31, 38, 53, 46, INK);
      tft.drawLine(98, 34, 74, 43, INK);
      tft.drawLine(97, 38, 75, 46, INK);
      if (blink) {
        openEye(tft, EYE_LX, true, INK);
        openEye(tft, EYE_RX, true, INK);
      } else {
        tft.fillCircle(EYE_LX, EYE_Y + 1, 6, INK);
        tft.fillCircle(EYE_RX, EYE_Y + 1, 6, INK);
        tft.fillRect(EYE_LX - 8, EYE_Y - 7, 17, 5, BG);  // heavy lids
        tft.fillRect(EYE_RX - 8, EYE_Y - 7, 17, 5, BG);
      }
      tft.fillRect(MOUTH_X - 12, 92, 25, 4, INK);  // flat mouth
      break;

    case MOOD_SLEEPY:
      arc(tft, EYE_LX, EYE_Y - 2, 7, 20, 160, INK, 3);  // closed, relaxed eyes
      arc(tft, EYE_RX, EYE_Y - 2, 7, 20, 160, INK, 3);
      tft.drawCircle(MOUTH_X, 90, 3, INK);  // tiny mouth
      // drifting Z's, rising a step per tick
      tft.setTextColor(ZBLUE, BG);
      tft.drawString("Z", 96, 40 - (tick % 3) * 3, 2);
      tft.drawString("Z", 108, 24 - (tick % 3) * 2, 4);
      break;

    default:
      break;
  }
}
