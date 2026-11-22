// =====================================================================
//  board_pins.h - every pin on the LOLIN S3 Mini Pro, in one place
// ---------------------------------------------------------------------
//  Sources (all LOLIN/WEMOS official):
//   * wemos.cc product docs: https://www.wemos.cc/en/latest/s3/s3_mini_pro.html
//   * Official schematic V1.0.0 (sch_s3_mini_pro_v1.0.0.pdf)
//   * LOLIN's own Arduino example: github.com/wemos/D1_mini_Examples,
//     examples/06.S3_MINI_PRO/s3_mini_pro/s3_mini_pro.ino
// =====================================================================
#pragma once

// ---- TFT display (0.85", 128x128, ST7789 or GC9A01 by batch) --------
// These are wired into TFT_eSPI through build flags in platformio.ini
// (mirrors LOLIN's TFT_eSPI Setup404 / Setup406). Listed here so the
// wiring lives in the source too:
//   MOSI 38   SCK 40   CS 35   DC 36   RST 34   BL 33 (backlight, HIGH = on)
#define TFT_BL_PIN 33

// ---- I2C bus (shared: IMU + the SH1.0-4P "I2C port" on the board) ---
#define I2C_SDA_PIN 12   // silkscreen "IO12/SDA" - LOLIN example: Wire.setPins(12, 11)
#define I2C_SCL_PIN 11   // silkscreen "IO11/SCL"

// ---- QMI8658C IMU ----------------------------------------------------
// Address 0x6B exactly as in LOLIN's official Arduino example.
// Interrupt lines (INT1 = 18, INT2 = 21) are not used in v1; we poll.
#define IMU_I2C_ADDR 0x6B
#define IMU_INT1_PIN 18
#define IMU_INT2_PIN 21

// ---- Buttons ----------------------------------------------------------
// Three front buttons, active LOW with the internal pull-up on.
// LOLIN's example names them left / OK / right.
#define BTN_LEFT_PIN  0    // left button - also the BOOT strapping pin:
                           // fine as a button once we're running, just
                           // don't hold it down while powering on!
#define BTN_OK_PIN    47   // middle button
#define BTN_RIGHT_PIN 48   // right button

// ---- RGB LED (WS2812B-0807) ------------------------------------------
#define RGB_LED_PIN 8      // data - driven with the core's neopixelWrite()
#define RGB_PWR_PIN 7      // power gate: must be HIGH or the LED stays dark

// ---- IR ---------------------------------------------------------------
#define IR_TX_PIN 9        // onboard IR LED transmitter.
// v1 does NOT send IR (digifight protocol is a later phase), this pin
// is only defined so nothing else ever claims it.

// RESERVED for the future IR receiver (digifights need a receiver -
// the board only ships with the transmitter LED above).
// A VS1838B's OUT pin will go here, powered from 3.3V (NEVER 5V).
// Nothing in v1 reads or drives this pin.
#define IR_RX_PIN 14

// GPIO13 is the header pin next to 14 - also left spare on purpose,
// as expansion room beside the future receiver.
