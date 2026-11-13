#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "BluetoothA2DPSource.h"
#include <math.h>

#define SPEAKER_NAME "X1"
#define VOLUME 100

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(33, 36);
BluetoothA2DPSource a2dp;

const int SR = 44100;
const float CYCLE = 1.6f;
const int CYCLES = 2;
volatile int32_t ringPos = -1;

#define C_BG    0x0841
#define C_RING  0x2124
#define C_BLUE  0x04FF
#define C_GREEN 0x07E0
#define C_WHITE 0xFFFF
#define C_GREY  0x8410

int32_t getData(Frame *data, int32_t count) {
  for (int i = 0; i < count; i++) {
    int16_t s = 0;
    int32_t p = ringPos;
    if (p >= 0) {
      float t = (float)p / SR;
      float lt = fmodf(t, CYCLE);
      float f = lt < 0.6f ? 880.0f : 660.0f;
      float nt = lt < 0.6f ? lt : lt - 0.6f;
      float env = expf(-3.0f * nt) * fminf(1.0f, nt / 0.01f);
      s = (int16_t)(12000 * env * sinf(2 * PI * f * nt));
      ringPos = (t >= CYCLE * CYCLES) ? -1 : p + 1;
    }
    data[i].channel1 = s;
    data[i].channel2 = s;
  }
  return count;
}

void drawStatus(bool bt) {
  tft.fillRect(0, 0, 320, 36, C_BG);
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawString("CALL STAFF", 14, 8, 4);
  tft.fillCircle(296, 18, 7, bt ? C_GREEN : TFT_ORANGE);
  tft.setTextColor(C_GREY, C_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(bt ? "Speaker OK" : "Connecting...", 280, 18, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawButton(bool ringing) {
  const int cx = 160, cy = 135;
  uint16_t main = ringing ? C_GREEN : C_BLUE;
  tft.fillCircle(cx, cy, 86, C_BG);
  tft.fillCircle(cx, cy, 84, C_RING);
  tft.fillCircle(cx, cy, 72, main);
  tft.drawCircle(cx, cy, 72, C_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_WHITE, main);
  tft.drawString(ringing ? "RINGING" : "PRESS", cx, cy - 8, 4);
  tft.drawString(ringing ? "..." : "TO CALL", cx, cy + 20, 2);
  tft.setTextDatum(TL_DATUM);
}

void setup() {
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  touchSPI.begin(25, 39, 32, 33);
  ts.begin(touchSPI);
  ts.setRotation(1);
  drawStatus(false);
  drawButton(false);
  a2dp.set_auto_reconnect(true);
  a2dp.set_data_callback_in_frames(getData);
  a2dp.set_volume(VOLUME);
  a2dp.start(SPEAKER_NAME);
}

void loop() {
  static bool lastBt = false, lastRing = false;
  static uint32_t lastCheck = 0;
  bool ringing = ringPos >= 0;
  if (ts.touched() && !ringing) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3700, 0, 320);
    int y = map(p.y, 240, 3800, 0, 240);
    int dx = x - 160, dy = y - 135;
    if (dx * dx + dy * dy < 90 * 90) ringPos = 0;
  }
  ringing = ringPos >= 0;
  if (ringing != lastRing) { lastRing = ringing; drawButton(ringing); }
  if (millis() - lastCheck > 500) {
    lastCheck = millis();
    bool bt = a2dp.is_connected();
    if (bt != lastBt) { lastBt = bt; drawStatus(bt); }
  }
  delay(10);
}
