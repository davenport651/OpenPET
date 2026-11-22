// =====================================================================
//  themepack.h - SRPK theme packs: storage, validation, rendering,
//  and the serial upload receiver (Sera-vice v2)
// ---------------------------------------------------------------------
//  A theme pack is a single binary file holding one 128x128 RGB565
//  portrait per mood. Packs live in the dedicated "themepack" flash
//  partition (see partitions.csv: 1 MB at offset 0x200000), wholly
//  separate from the firmware and from NVS. Updating one never
//  touches the other.
//
//  If no valid pack is found at boot, the firmware quietly uses its
//  built-in TFT-primitive faces (faces.h) - the pack is a skin,
//  never a single point of failure.
//
// ---------------------------------------------------------------------
//  SRPK FORMAT v1 (all integers little-endian)
// ---------------------------------------------------------------------
//    offset  size  field
//    0       4     magic "SRPK"
//    4       2     format version (= 1)
//    6       2     header size in bytes (= 64 + 4 * frameCount)
//    8       4     total pack size in bytes (header + payload)
//    12      4     CRC32 of the payload (zlib crc32: polynomial
//                  0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF,
//                  i.e. exactly Python's zlib.crc32 / binascii.crc32)
//    16      1     frame count (a full mood pack has 8)
//    17      1     pixel format (1 = RGB565, one uint16 per pixel,
//                  value (R>>3)<<11 | (G>>2)<<5 | B>>3, stored HIGH
//                  byte first: pushImage() consumes the buffer
//                  big-endian on this stack, so low-byte-first packs
//                  display byte-swapped - verified on hardware
//                  2026-11-06; the fix lives in build_pack.py)
//    18      2     frame width  (128)
//    20      2     frame height (128)
//    22      26    pack name, NUL-padded ASCII ("chibi-sera")
//    48      16    reserved, zero
//    64      4*N   frame offset table: N uint32 offsets from the
//                  start of the pack to each frame's data
//    headerSize..  payload: N frames back to back, each
//                  width*height*2 bytes (32768 for 128x128),
//                  row-major, top row first, in Mood enum order
//                  (Content, Happy, Thinking, Excited, Smug,
//                  Worried, Grumpy, Sleepy - see faces.h).
//  Worked example (the "chibi-sera" pack): headerSize = 96,
//  payload = 8 * 32768 = 262144 bytes, totalSize = 262240 bytes.
//
// ---------------------------------------------------------------------
//  UPLOAD (serial, sender-agnostic)
// ---------------------------------------------------------------------
//  The serial reader watches the raw byte stream for the "SRPK"
//  magic; every other byte goes to the normal command handler (the
//  matcher holds a partial magic prefix back and replays it as
//  commands if the match breaks, so single-letter commands are
//  unaffected). Once the magic is seen, the announced pack is staged
//  in PSRAM and CRC-verified BEFORE the partition is erased - a
//  truncated or corrupt transfer leaves the old pack (or built-in
//  faces) completely untouched. On success it prints
//  "PACK OK <name>", on any failure "PACK FAIL <reason>".
//  Because the sender never has to handshake, this works with:
//      stty -F /dev/ttyACM0 115200 raw -echo
//      cat chibi-sera.pack.bin > /dev/ttyACM0
//  A gap of more than PACK_RX_IDLE_TIMEOUT_MS with no bytes aborts
//  the transfer (PACK FAIL timeout). Writing the verified pack to
//  flash takes a couple of seconds; she pauses, then shows the new
//  face for the current mood.
// =====================================================================
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "esp_partition.h"
#include "faces.h"  // for the Mood enum + MOOD_COUNT

namespace ThemePack {

const uint16_t FORMAT_VERSION = 1;
const uint8_t PIXEL_FORMAT_RGB565 = 1;
const int FRAME_W = 128;
const int FRAME_H = 128;
const size_t FRAME_BYTES = FRAME_W * FRAME_H * 2;  // 32768
const size_t BASE_HEADER = 64;                     // bytes before the offset table
const uint32_t RX_IDLE_TIMEOUT_MS = 5000;

// ---- CRC32 (zlib semantics; chainable: pass the previous return) ---
inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0) {
  uint32_t c = crc ^ 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
  }
  return c ^ 0xFFFFFFFFu;
}

// ---- little-endian field readers ------------------------------------
inline uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
inline uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

// ---- state ------------------------------------------------------------
const esp_partition_t* g_part = nullptr;
bool g_active = false;
char g_name[27] = "";
uint8_t g_frameCount = 0;
uint32_t g_offsets[MOOD_COUNT] = {0};
uint16_t* g_frameBuf = nullptr;  // one frame, staged in PSRAM

// Validate header fields (pointed to by h, at least BASE_HEADER bytes
// available in the source). Fills frameCountOut/headerSizeOut/
// totalSizeOut/nameOut. Does NOT check the payload CRC.
inline bool parseHeader(const uint8_t* h, uint8_t& frameCountOut,
                        uint16_t& headerSizeOut, uint32_t& totalSizeOut,
                        char* nameOut) {
  if (memcmp(h, "SRPK", 4) != 0) return false;
  if (rd16(h + 4) != FORMAT_VERSION) return false;
  uint8_t frames = h[16];
  if (frames < MOOD_COUNT || frames > 16) return false;
  if (h[17] != PIXEL_FORMAT_RGB565) return false;
  if (rd16(h + 18) != FRAME_W || rd16(h + 20) != FRAME_H) return false;
  uint16_t hsize = rd16(h + 6);
  if (hsize != BASE_HEADER + 4 * frames) return false;
  uint32_t total = rd32(h + 8);
  if (total != hsize + (uint32_t)frames * FRAME_BYTES) return false;
  frameCountOut = frames;
  headerSizeOut = hsize;
  totalSizeOut = total;
  if (nameOut) {
    memcpy(nameOut, h + 22, 26);
    nameOut[26] = '\0';
  }
  return true;
}

// Read the pack currently in the partition and validate it fully
// (header + payload CRC). On success the pack becomes active.
inline bool loadFromPartition() {
  g_active = false;
  if (!g_part) return false;
  uint8_t hdr[BASE_HEADER + 4 * 16];
  if (esp_partition_read(g_part, 0, hdr, sizeof(hdr)) != ESP_OK) return false;
  uint8_t frames;
  uint16_t hsize;
  uint32_t total;
  char name[27];
  if (!parseHeader(hdr, frames, hsize, total, name)) return false;
  if (total > g_part->size) return false;
  // CRC the payload, streaming from flash in 4 KB chunks.
  uint32_t crc = 0;
  uint8_t chunk[4096];
  uint32_t off = hsize;
  while (off < total) {
    size_t n = total - off;
    if (n > sizeof(chunk)) n = sizeof(chunk);
    if (esp_partition_read(g_part, off, chunk, n) != ESP_OK) return false;
    crc = crc32(chunk, n, crc);
    off += n;
  }
  if (crc != rd32(hdr + 12)) return false;
  for (int i = 0; i < MOOD_COUNT; i++) g_offsets[i] = rd32(hdr + BASE_HEADER + 4 * i);
  g_frameCount = frames;
  memcpy(g_name, name, sizeof(g_name));
  g_active = true;
  return true;
}

// Call once from setup(). Safe when no pack / no partition exists.
inline bool init() {
  g_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_ANY, "themepack");
  if (!g_part) {
    Serial.println("[pack] themepack partition NOT FOUND - built-in faces");
    return false;
  }
  if (loadFromPartition()) {
    Serial.printf("[pack] active: \"%s\" (%u frames)\n", g_name, g_frameCount);
    return true;
  }
  Serial.println("[pack] no valid pack in partition - built-in faces");
  return false;
}

inline bool active() { return g_active; }
inline const char* name() { return g_name; }

// Draw mood's portrait fullscreen. Returns false if no usable frame,
// so the caller can fall back to the primitive face.
inline bool drawFrame(TFT_eSPI& tft, uint8_t moodIndex) {
  if (!g_active || moodIndex >= MOOD_COUNT) return false;
  if (!g_frameBuf) {
    g_frameBuf = (uint16_t*)ps_malloc(FRAME_BYTES);
    if (!g_frameBuf) g_frameBuf = (uint16_t*)malloc(FRAME_BYTES);
    if (!g_frameBuf) return false;
  }
  if (esp_partition_read(g_part, g_offsets[moodIndex], g_frameBuf, FRAME_BYTES) != ESP_OK)
    return false;
  tft.pushImage(0, 0, FRAME_W, FRAME_H, g_frameBuf);
  return true;
}

// =====================================================================
//  Serial receiver
// =====================================================================
enum RxState : uint8_t { RX_SCAN, RX_STAGE };
RxState g_rxState = RX_SCAN;
uint8_t g_magicHeld = 0;        // bytes of "SRPK" prefix matched so far
uint8_t* g_stage = nullptr;     // PSRAM staging buffer (partition-sized)
size_t g_staged = 0;
uint32_t g_stageTotal = 0;      // announced total size (0 until parsed)
unsigned long g_lastByteAt = 0;
bool g_installedEvent = false;

typedef void (*CommandByteHandler)(uint8_t b);

inline bool receiving() { return g_rxState == RX_STAGE; }

// Consume the install event (a new pack was written + activated).
inline bool takeInstalledEvent() {
  bool e = g_installedEvent;
  g_installedEvent = false;
  return e;
}

inline void rxFail(const char* reason) {
  Serial.printf("PACK FAIL %s\n", reason);
  g_rxState = RX_SCAN;
  g_staged = 0;
  g_stageTotal = 0;
  g_magicHeld = 0;
}

// The staged bytes are complete and CRC-verified: erase + write the
// partition, then activate from what's actually on the flash.
inline void rxCommit() {
  uint32_t total = g_stageTotal;
  uint32_t eraseSpan = (total + 0xFFF) & ~0xFFFu;
  if (esp_partition_erase_range(g_part, 0, eraseSpan) != ESP_OK) {
    rxFail("flash erase");
    return;
  }
  if (esp_partition_write(g_part, 0, g_stage, total) != ESP_OK) {
    rxFail("flash write");
    return;
  }
  if (!loadFromPartition()) {
    rxFail("verify after write");
    return;
  }
  Serial.printf("PACK OK %s\n", g_name);
  g_installedEvent = true;
  g_rxState = RX_SCAN;
  g_staged = 0;
  g_stageTotal = 0;
  g_magicHeld = 0;
}

inline void rxStageByte(uint8_t b) {
  if (!g_part) { rxFail("no partition"); return; }
  if (!g_stage) {
    g_stage = (uint8_t*)ps_malloc(g_part->size);
    if (!g_stage) g_stage = (uint8_t*)malloc(g_part->size);
    if (!g_stage) { rxFail("out of memory"); return; }
  }
  if (g_staged >= g_part->size) { rxFail("oversize"); return; }
  g_stage[g_staged++] = b;
  g_lastByteAt = millis();

  // Parse the announced total size as soon as it has arrived.
  if (g_stageTotal == 0 && g_staged >= 12) {
    g_stageTotal = rd32(g_stage + 8);
    if (g_stageTotal < BASE_HEADER + FRAME_BYTES || g_stageTotal > g_part->size) {
      rxFail("bad size");
      return;
    }
    Serial.printf("[pack] receiving %u bytes...\n", (unsigned)g_stageTotal);
  }
  if (g_stageTotal == 0 || g_staged < g_stageTotal) return;

  // Complete: validate header + payload CRC before touching flash.
  uint8_t frames;
  uint16_t hsize;
  uint32_t total;
  char nm[27];
  if (!parseHeader(g_stage, frames, hsize, total, nm) || total != g_stageTotal) {
    rxFail("bad header");
    return;
  }
  if (crc32(g_stage + hsize, total - hsize) != rd32(g_stage + 12)) {
    rxFail("bad crc");
    return;
  }
  rxCommit();
}

// Feed one raw serial byte. Bytes that are not part of a pack are
// handed to onCommandByte (held magic-prefix bytes are replayed
// there if the match breaks, so commands see the stream unchanged).
inline void feedSerial(uint8_t b, CommandByteHandler onCommandByte) {
  static const char MAGIC[4] = {'S', 'R', 'P', 'K'};
  if (g_rxState == RX_STAGE) {
    rxStageByte(b);
    return;
  }
  if (b == (uint8_t)MAGIC[g_magicHeld]) {
    g_magicHeld++;
    if (g_magicHeld == 4) {
      g_rxState = RX_STAGE;
      g_staged = 0;
      g_stageTotal = 0;
      g_lastByteAt = millis();
      for (int i = 0; i < 4; i++) rxStageByte((uint8_t)MAGIC[i]);
    }
    return;
  }
  // Match broke: replay whatever prefix we were holding, then treat
  // this byte on its own merits (it may itself start a new match).
  for (uint8_t i = 0; i < g_magicHeld; i++) onCommandByte((uint8_t)MAGIC[i]);
  g_magicHeld = 0;
  if (b == (uint8_t)MAGIC[0]) {
    g_magicHeld = 1;
  } else {
    onCommandByte(b);
  }
}

// Call every loop(): aborts a transfer whose sender went silent.
inline void pollTimeouts() {
  if (g_rxState == RX_STAGE && millis() - g_lastByteAt > RX_IDLE_TIMEOUT_MS) {
    rxFail("timeout");
  }
}

}  // namespace ThemePack
