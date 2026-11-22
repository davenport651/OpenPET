// =====================================================================
//  Sera-vice firmware v2 - LOLIN S3 Mini Pro
// ---------------------------------------------------------------------
//  A tiny desk-pet Sera: a face on the 128x128 TFT, a mood-colored
//  RGB LED, and an IMU so she reacts to being picked up, shaken and
//  flipped. Mood design lives in ../mood-chart.md; this file is the
//  engine that runs it.
//
//  v2 adds theme packs (themepack.h): if a valid SRPK pack sits in
//  the "themepack" flash partition, its portraits replace the drawn
//  faces mood for mood; if not, the v1 primitive faces below are the
//  built-in fallback, unchanged. Packs upload over this same serial
//  port with a plain `cat pack.bin > /dev/ttyACM0` (see themepack.h).
//
//  Still local-only, as in v1: no Wi-Fi, no IR battles (the board
//  has an IR *transmitter* but no receiver - that waits for a
//  VS1838B on GPIO14, see board_pins.h), no Muse pairing, and no
//  sound (the board has no buzzer - jingles are data + a stub
//  player, see jingles.h).
//
//  Controls:
//    Left button (GPIO0)    hello! -> Happy
//    OK button (GPIO47)     cycle: Thinking -> Smug -> Worried -> auto
//    Right button (GPIO48)  same cycle, backwards
//    Shake her              -> Excited
//    Flip her face-down     -> Grumpy
//    Leave her alone ~3 min -> Sleepy; pick her up -> Happy
//    Serial (115200)        c/h/t/e/s/w/g/z pick a mood, ? = help
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <FastIMU.h>

#include "board_pins.h"
#include "faces.h"
#include "jingles.h"
#include "themepack.h"

// ---------------------------------------------------------------------
//  Tunables - the knobs Marcus will actually want to turn
// ---------------------------------------------------------------------
#define SLEEP_AFTER_MS 180000UL  // still this long -> Sleepy (3 minutes)
#define FLIP_HOLD_MS 600UL       // face-down this long -> Grumpy
#define GRUMP_COOLDOWN_MS 8000UL
#define EXCITED_COOLDOWN_MS 4000UL
#define LED_BRIGHTNESS 0.35f     // global LED dimmer (the WS2812 is BRIGHT)

// ---------------------------------------------------------------------
//  Hardware objects
// ---------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
QMI8658 imu;
calData calib = {0};  // zeroed = uncalibrated; fine for tilt/shake tricks
AccelData accelData;
GyroData gyroData;
bool imuOk = false;

// ---------------------------------------------------------------------
//  Mood engine state
// ---------------------------------------------------------------------
Mood mood = MOOD_CONTENT;
bool moodSticky = true;            // sticky moods don't time out
unsigned long moodExpiresAt = 0;   // for transient moods
int animTick = 0;

unsigned long lastMotionAt = 0;    // last IMU/button/serial activity
unsigned long lastGrumpAt = 0;
unsigned long lastExcitedAt = 0;

// LED behaviour per mood, from the chart: {r, g, b, mode, period ms}
// mode: 0 = steady, 1 = breathing (slow sine), 2 = pulse (snappier)
struct LedSpec { uint8_t r, g, b, mode; uint16_t periodMs; };
const LedSpec LED_FOR[MOOD_COUNT] = {
    {20, 180, 50, 1, 4000},    // Content:  slow breathing green
    {40, 255, 80, 2, 900},     // Happy:    warm green pulse
    {30, 80, 255, 1, 3000},    // Thinking: breathing blue
    {140, 255, 150, 2, 380},   // Excited:  quick bright pulse
    {0, 190, 170, 0, 0},       // Smug:     steady teal
    {255, 130, 15, 2, 1300},   // Worried:  soft orange pulse
    {255, 160, 0, 0, 0},       // Grumpy:   amber
    {15, 30, 140, 1, 5200},    // Sleepy:   dim blue slow pulse
};

// The jingle the chart assigns to entering each mood (nullptr = silent)
const char* jingleFor(Mood m) {
  switch (m) {
    case MOOD_HAPPY:   return Jingles::GREETING;
    case MOOD_EXCITED: return Jingles::EXCITED;
    case MOOD_SMUG:    return Jingles::SMUG;
    case MOOD_WORRIED: return Jingles::ALERT;
    case MOOD_GRUMPY:  return Jingles::DISAPPOINTED;
    case MOOD_SLEEPY:  return Jingles::LULLABY;
    default:           return nullptr;  // Content & Thinking are silent
  }
}

// A valid theme pack wins over the drawn faces for normal display.
// Blinks and animation ticks are fallback-face business: a portrait
// is a finished picture, so while a pack is active we only redraw on
// mood changes (the loop's blink/tick blocks are pack-aware too).
void drawCurrentFace(bool blink = false) {
  if (ThemePack::active()) {
    if (blink) return;  // portraits are finished pictures; they don't blink
    if (ThemePack::drawFrame(tft, (uint8_t)mood)) return;
    // active but unreadable: fall through to the built-in face
  }
  drawFace(tft, mood, blink, animTick);
}

// Enter a mood. holdMs = 0 means "sticky" (stays until something else
// happens); otherwise she returns to her rest state when it lapses.
void enterMood(Mood m, unsigned long holdMs) {
  mood = m;
  moodSticky = (holdMs == 0);
  moodExpiresAt = millis() + holdMs;
  animTick = 0;
  const char* j = jingleFor(m);
  if (j) playJingle(j);  // silent stub on this hardware - see jingles.h
  drawCurrentFace();
  Serial.printf("[mood] %s%s\n", moodName(m), moodSticky ? "" : " (transient)");
}

// Where she settles when a transient mood lapses: asleep if she's been
// ignored long enough, Content otherwise.
Mood restMood() {
  return (millis() - lastMotionAt > SLEEP_AFTER_MS) ? MOOD_SLEEPY : MOOD_CONTENT;
}

// ---------------------------------------------------------------------
//  Buttons - simple polled debounce (LOLIN's example uses interrupts;
//  polling is easier to follow and plenty fast for a desk pet)
// ---------------------------------------------------------------------
struct Button {
  uint8_t pin;
  bool lastRead = HIGH;
  bool stable = HIGH;
  unsigned long changedAt = 0;
  bool poll() {  // returns true once per press
    bool now = digitalRead(pin);
    if (now != lastRead) { lastRead = now; changedAt = millis(); }
    if (now != stable && millis() - changedAt > 25) {
      stable = now;
      return stable == LOW;  // buttons are active LOW
    }
    return false;
  }
};
Button btnLeft{BTN_LEFT_PIN}, btnOk{BTN_OK_PIN}, btnRight{BTN_RIGHT_PIN};

// Manual mood ring for the OK / Right buttons: Thinking -> Smug ->
// Worried -> back to auto (Content). -1 = auto.
int ringIndex = -1;
const Mood RING[] = {MOOD_THINKING, MOOD_SMUG, MOOD_WORRIED};
const int RING_LEN = 3;

void ringStep(int dir) {
  ringIndex += dir;
  if (ringIndex >= RING_LEN) ringIndex = -1;
  if (ringIndex < -1) ringIndex = RING_LEN - 1;
  if (ringIndex < 0) enterMood(MOOD_CONTENT, 0);
  else enterMood(RING[ringIndex], 0);
}

void noteActivity() { lastMotionAt = millis(); }

// Any deliberate interaction: also her wake-up path out of Sleepy.
void onButtonGreet() {
  noteActivity();
  ringIndex = -1;
  enterMood(MOOD_HAPPY, 3500);
}

// ---------------------------------------------------------------------
//  Serial commands - handy for testing faces from a desk chair
// ---------------------------------------------------------------------
void printHelp() {
  Serial.println("Sera-vice v2 commands: c=Content h=Happy t=Thinking "
                 "e=Excited s=Smug w=Worried g=Grumpy z=Sleepy i=IMU dump ?=help");
  Serial.println("Theme pack: send an SRPK .pack.bin file over this port "
                 "(e.g. cat pack.bin > /dev/ttyACM0) to install it.");
}

void onSerialCommand(char c) {
  noteActivity();
  switch (c) {
    case 'c': ringIndex = -1; enterMood(MOOD_CONTENT, 0); break;
    case 'h': enterMood(MOOD_HAPPY, 3500); break;
    case 't': enterMood(MOOD_THINKING, 5000); break;
    case 'e': enterMood(MOOD_EXCITED, 3000); break;
    case 's': enterMood(MOOD_SMUG, 4000); break;
    case 'w': enterMood(MOOD_WORRIED, 6000); break;
    case 'g': enterMood(MOOD_GRUMPY, 4000); break;
    case 'z': enterMood(MOOD_SLEEPY, 0); break;
    case 'i':
      Serial.printf("[imu] a=(%.2f, %.2f, %.2f) g=(%.1f, %.1f, %.1f) ok=%d\n",
                    accelData.accelX, accelData.accelY, accelData.accelZ,
                    gyroData.gyroX, gyroData.gyroY, gyroData.gyroZ, imuOk);
      break;
    case '?': printHelp(); break;
    default: break;
  }
}

// Byte-level entry point the theme-pack stream watcher hands every
// non-pack byte to (see themepack.h): exactly v1's direct-read
// behaviour, case-folding included.
void serialCommandByte(uint8_t b) {
  char c = (char)b;
  if (c >= 'A' && c <= 'Z') c += 32;  // accept capitals too
  onSerialCommand(c);
}

// ---------------------------------------------------------------------
//  IMU triggers: shake -> Excited, face-down -> Grumpy,
//  stillness -> Sleepy, motion after sleep -> Happy.
//  Axis note (from LOLIN's factory IMU demo on this board): face-up on
//  a desk reads accel Z ~= -1 g, so face-DOWN reads Z ~= +1 g.
// ---------------------------------------------------------------------
float prevAx = 0, prevAy = 0, prevAz = -1;
int shakeHits = 0;
unsigned long lastShakeAt = 0;
unsigned long flipStartAt = 0;

void pollImu() {
  if (!imuOk) return;
  imu.update();
  imu.getAccel(&accelData);
  imu.getGyro(&gyroData);
  float ax = accelData.accelX, ay = accelData.accelY, az = accelData.accelZ;
  unsigned long now = millis();

  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float gyroMag = sqrtf(gyroData.gyroX * gyroData.gyroX +
                        gyroData.gyroY * gyroData.gyroY +
                        gyroData.gyroZ * gyroData.gyroZ);
  float jerk = sqrtf((ax - prevAx) * (ax - prevAx) + (ay - prevAy) * (ay - prevAy) +
                     (az - prevAz) * (az - prevAz));
  prevAx = ax; prevAy = ay; prevAz = az;

  // --- motion = activity (and her alarm clock while Sleepy) ---
  if (fabsf(mag - 1.0f) > 0.15f || gyroMag > 30.0f) {
    noteActivity();
    if (mood == MOOD_SLEEPY) {  // picked up! (chart: motion after sleep = Happy)
      ringIndex = -1;
      enterMood(MOOD_HAPPY, 3000);
      return;
    }
  }

  // --- shake: several sharp jerks inside a short window ---
  if (jerk > 0.5f) { shakeHits++; lastShakeAt = now; }
  if (now - lastShakeAt > 1200) shakeHits = 0;
  if (shakeHits >= 4 && now - lastExcitedAt > EXCITED_COOLDOWN_MS) {
    shakeHits = 0;
    lastExcitedAt = now;
    enterMood(MOOD_EXCITED, 3000);
    return;
  }

  // --- flipped face-down, and left there a moment ---
  if (az > 0.55f) {
    if (flipStartAt == 0) flipStartAt = now;
    if (now - flipStartAt > FLIP_HOLD_MS && now - lastGrumpAt > GRUMP_COOLDOWN_MS &&
        mood != MOOD_GRUMPY) {
      lastGrumpAt = now;
      enterMood(MOOD_GRUMPY, 4000);
    }
  } else if (az < 0.3f) {
    flipStartAt = 0;
  }
}

// ---------------------------------------------------------------------
//  RGB LED - per-mood color with breathing / pulse envelopes
// ---------------------------------------------------------------------
void updateLed() {
  const LedSpec& s = LED_FOR[mood];
  float env = 1.0f;
  if (s.mode == 1) {  // breathing
    env = 0.3f + 0.7f * (0.5f - 0.5f * cosf(6.2832f * (millis() % s.periodMs) / s.periodMs));
  } else if (s.mode == 2) {  // pulse
    float ph = 0.5f - 0.5f * cosf(6.2832f * (millis() % s.periodMs) / s.periodMs);
    env = ph * ph;
  }
  float k = LED_BRIGHTNESS * env;
  neopixelWrite(RGB_LED_PIN, (uint8_t)(s.r * k), (uint8_t)(s.g * k), (uint8_t)(s.b * k));
}

// ---------------------------------------------------------------------
//  Boot splash
// ---------------------------------------------------------------------
void splash() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SERA", 64, 44, 4);
  tft.setTextColor(tft.color565(140, 200, 255), TFT_BLACK);
  tft.drawString("sera-vice v2", 64, 70, 2);
#ifdef SERA_PANEL_GC9A01
  tft.drawString("panel: GC9A01", 64, 88, 2);
#elif defined(SERA_PANEL_ST7789)
  tft.drawString("panel: ST7789", 64, 88, 2);
#endif
  tft.drawString(imuOk ? "IMU ok" : "IMU missing!", 64, 104, 2);
  // LED sweep, same trick as LOLIN's factory demo
  const uint8_t sweep[3][3] = {{60, 0, 0}, {0, 60, 0}, {0, 0, 60}};
  for (auto& c : sweep) { neopixelWrite(RGB_LED_PIN, c[0], c[1], c[2]); delay(160); }
  neopixelWrite(RGB_LED_PIN, 0, 0, 0);
  delay(900);
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // RGB LED power gate first, or the LED ignores us completely
  pinMode(RGB_PWR_PIN, OUTPUT);
  digitalWrite(RGB_PWR_PIN, HIGH);
  pinMode(RGB_LED_PIN, OUTPUT);

  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_OK_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);

  // I2C exactly as LOLIN's example brings it up: SDA 12, SCL 11, 400 kHz
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.begin();
  Wire.setClock(400000);

  tft.init();
  tft.setRotation(0);

  // Theme pack check: a valid SRPK pack in its partition takes over
  // the faces; anything else and the built-in faces run (see
  // themepack.h). Needs nothing but the flash, so it goes first.
  ThemePack::init();

  int err = imu.init(calib, IMU_I2C_ADDR);
  imuOk = (err == 0);
  if (imuOk) {
    imu.update();
    imu.getAccel(&accelData);
    prevAx = accelData.accelX; prevAy = accelData.accelY; prevAz = accelData.accelZ;
  }

  splash();
  lastMotionAt = millis();
  enterMood(MOOD_CONTENT, 0);

  Serial.println("Sera-vice v2 booted.");
#ifdef SERA_PANEL_GC9A01
  Serial.println("Panel build: GC9A01");
#elif defined(SERA_PANEL_ST7789)
  Serial.println("Panel build: ST7789");
#endif
  Serial.println(imuOk ? "IMU: QMI8658C ok" : "IMU: NOT FOUND - motion triggers disabled");
  printHelp();
}

// ---------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // --- inputs ---
  if (btnLeft.poll()) onButtonGreet();
  if (btnOk.poll()) { noteActivity(); ringStep(+1); }
  if (btnRight.poll()) { noteActivity(); ringStep(-1); }
  // Serial goes through the theme-pack watcher: bytes that turn out
  // to be an SRPK upload are staged + verified by themepack.h, all
  // others land in serialCommandByte() as ordinary commands.
  while (Serial.available()) {
    int b = Serial.read();
    if (b < 0) break;
    ThemePack::feedSerial((uint8_t)b, serialCommandByte);
  }
  ThemePack::pollTimeouts();
  if (ThemePack::takeInstalledEvent()) drawCurrentFace();  // new pack: show her face

  // --- IMU at ~16 Hz (paused while a pack streams in: draining the
  // serial buffer promptly matters more than motion for a few seconds)
  static unsigned long lastImuAt = 0;
  if (now - lastImuAt > 60 && !ThemePack::receiving()) { lastImuAt = now; pollImu(); }

  // --- transient mood expiry ---
  if (!moodSticky && now >= moodExpiresAt) {
    if (mood == MOOD_THINKING) playJingle(Jingles::DONE);  // chart: Done chirp on completion
    enterMood(restMood(), 0);
  }

  // --- long stillness -> Sleepy (checked last; motion resets it) ---
  if (mood != MOOD_SLEEPY && moodSticky && now - lastMotionAt > SLEEP_AFTER_MS) {
    enterMood(MOOD_SLEEPY, 0);
  }

  // --- animation: blinks for open-eyed moods, ticks for Sleepy/Excited.
  // Both belong to the drawn faces; with a theme pack active the
  // portrait is static and neither applies (mood changes redraw).
  static unsigned long nextBlinkAt = 2500;
  static bool blinking = false;
  static unsigned long blinkEndAt = 0;
  static unsigned long lastAnimAt = 0;
  bool packOn = ThemePack::active();
  bool openEyed = !packOn && (mood == MOOD_CONTENT || mood == MOOD_THINKING ||
                   mood == MOOD_WORRIED || mood == MOOD_GRUMPY);
  if (openEyed && !blinking && now >= nextBlinkAt) {
    blinking = true;
    blinkEndAt = now + 130;
    drawCurrentFace(true);
  }
  if (blinking && now >= blinkEndAt) {
    blinking = false;
    nextBlinkAt = now + 2600 + (now % 1700);  // 2.6-4.3 s, vaguely organic
    drawCurrentFace(false);
  }
  if (!packOn && ((mood == MOOD_SLEEPY && now - lastAnimAt > 900) ||
      (mood == MOOD_EXCITED && now - lastAnimAt > 450))) {
    lastAnimAt = now;
    animTick++;
    if (!blinking) drawCurrentFace(false);
  }

  // --- LED at ~30 Hz ---
  static unsigned long lastLedAt = 0;
  if (now - lastLedAt > 30) { lastLedAt = now; updateLed(); }
}
