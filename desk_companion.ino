/*
  Desk Companion - Expressive OLED desk robot
  Board : ESP32-C3 Super Mini  (Arduino IDE: "ESP32C3 Dev Module", USB CDC On Boot = Enabled)
  Parts : 0.96" SSD1306 or 1.3" SH1106 I2C OLED + TTP223 touch sensor
  Libs  : U8g2 (olikraus)

  Features
    - Animated eyes: blink, look around, 13 expressions + sleeping
    - Touch: tap = next expression, double tap = toggle emotional mode,
             long press (3 s) = love (hearts)
    - Emotional mode: mood changes on its own
    - Music detection: companion script on laptop sends status via USB Serial
    - Sleep mode: screen off after 5 min idle, wakes on touch
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

// ======================= HARDWARE CONFIG (edit to match your wiring) =======================
#define USE_SH1106 0          // 0 = 0.96" SSD1306, 1 = 1.3" SH1106
const int PIN_SDA   = 8;
const int PIN_SCL   = 9;
const int PIN_TOUCH = 7;     // TTP223 OUT (some boards only work on GPIO 5, others on 7)
// ===========================================================================================

#if USE_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

Preferences prefs;

// ---------------------------------- settings ----------------------------------
struct Settings {
  bool emotional = true;
} cfg;

void loadSettings() {
  prefs.begin("companion", true);
  cfg.emotional = prefs.getBool("emo", true);
  prefs.end();
}

void saveSettings() {
  prefs.begin("companion", false);
  prefs.putBool("emo", cfg.emotional);
  prefs.end();
}

// ---------------------------------- expressions ----------------------------------
enum Expr : uint8_t {
  E_NEUTRAL, E_HAPPY, E_SAD, E_ANGRY, E_SLEEPY, E_SURPRISED, E_LOVE,
  E_WINK, E_DIZZY, E_SKEPTICAL, E_SHY, E_COOL, E_MUSIC,
  E_SLEEP, E_COUNT
};

struct Face { float w, h, tilt, happy, lid, love, closed, wink, dizzy, squint; };

const Face PRESETS[E_COUNT] = {
  //  w    h    tilt  happy   lid   love  closed wink  dizzy squint
  { 30,  28,    0,     0,     0,     0,    0,    0,    0,    0   },  // neutral
  { 30,  28,    0,     1,     0,     0,    0,    0,    0,    0   },  // happy
  { 30,  26,   -1,     0,     0,     0,    0,    0,    0,    0   },  // sad
  { 30,  26,    1,     0,     0,     0,    0,    0,    0,    0   },  // angry
  { 30,  28,    0,     0,  0.55f,    0,    0,    0,    0,    0   },  // sleepy
  { 36,  36,    0,     0,     0,     0,    0,    0,    0,    0   },  // surprised
  { 30,  28,    0,     0,     0,     1,    0,    0,    0,    0   },  // love (hearts)
  { 30,  28,    0,  0.5f,     0,     0,    0,    1,    0,    0   },  // wink
  { 30,  28,    0,     0,     0,     0,    0,    0,    1,    0   },  // dizzy (X eyes)
  { 32,  28, 0.4f,     0,     0,     0,    0, 0.4f,    0,    0   },  // skeptical
  { 22,  20,    0,     0,     0,     0,    0,    0,    0,    0   },  // shy (small eyes)
  { 30,  14,    0,  0.3f,     0,     0,    0,    0,    0,    1   },  // cool (narrow squint)
  { 30,  28,    0,  0.7f,     0,     0,    0,    0,    0,    0   },  // music (happy + notes)
  { 30,  28,    0,     0,     0,     0,    1,    0,    0,    0   },  // sleeping
};

// ---------------------------------- state ----------------------------------
Expr     curExpr = E_NEUTRAL;
uint32_t exprUntil = 0, nextMoodAt = 0;
Face     cur, tgt;
float    lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
uint32_t nextLookAt = 0, blinkStart = 0, nextBlinkAt = 0;
uint32_t lastFrame = 0, prevUpdateTime = 0;
uint32_t lastTouchAt = 0;
bool     sleeping = false;
bool     musicPlaying = false;
int      tapIdx = 0;

const uint32_t FRAME_MS = 40;  // ~25 FPS

// ---------------------------------- helpers ----------------------------------
void centerStr(const char* s, int y) {
  int x = (128 - u8g2.getUTF8Width(s)) / 2;
  if (x < 0) x = 0;
  u8g2.drawUTF8(x, y, s);
}

void bootText(const char* a, const char* b = "", const char* c = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  centerStr(a, 20);
  centerStr(b, 36);
  centerStr(c, 52);
  u8g2.sendBuffer();
}

void setExpr(Expr e, uint32_t holdMs = 0) {
  curExpr = e;
  tgt = PRESETS[e];
  exprUntil = holdMs ? millis() + holdMs : 0;
}

// ---------------------------------- drawing ----------------------------------
void drawHeart(int cx, int cy, int s) {
  u8g2.drawDisc(cx - s / 2, cy - s / 3, s / 2 + 1);
  u8g2.drawDisc(cx + s / 2, cy - s / 3, s / 2 + 1);
  u8g2.drawTriangle(cx - s - 1, cy - s / 3 + 2, cx + s + 1, cy - s / 3 + 2, cx, cy + s);
}

void drawHeadphones(int y_center) {
  // Earcups (left and right)
  u8g2.drawRBox(10, y_center - 12, 12, 28, 3);
  u8g2.drawRBox(106, y_center - 12, 12, 28, 3);
  
  // Headband vertical sides
  u8g2.drawBox(15, 6, 4, y_center - 12 - 6);
  u8g2.drawBox(109, 6, 4, y_center - 12 - 6);
  
  // Headband top horizontal arc
  u8g2.drawBox(17, 2, 94, 4);
  u8g2.drawBox(15, 4, 4, 4); // left corner curve
  u8g2.drawBox(109, 4, 4, 4); // right corner curve
}

void drawEye(int x, int y, int w, int h, bool left) {
  // squint: narrows both eyes
  if (cur.squint > 0.02f) {
    int sq = (int)(cur.squint * h * 0.5f);
    h -= sq;
    y += sq / 2;
    if (h < 4) h = 4;
  }

  // wink: progressively close left eye only
  if (left && cur.wink > 0.02f) {
    int wr = (int)(cur.wink * h * 0.9f);
    h -= wr;
    y += wr / 2;
    if (h < 3) {                                       // fully closed → flat line
      u8g2.drawRBox(x, y + h / 2 - 1, w, 3, 1);
      return;
    }
  }

  int r = h / 2;
  if (r > w / 2) r = w / 2;
  if (r > 10) r = 10;
  u8g2.setDrawColor(1);
  if (r > 0) u8g2.drawRBox(x, y, w, h, r);
  else       u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(0);

  if (cur.tilt > 0.05f) {                                     // angry: inner corners drop
    int d = (int)(cur.tilt * h * 0.55f);
    if (left) u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d);
    else      u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d);
  } else if (cur.tilt < -0.05f) {                             // sad: outer corners drop
    int d = (int)(-cur.tilt * h * 0.55f);
    if (left) u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d);
    else      u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d);
  }

  if (cur.lid > 0.02f)                                        // sleepy lid
    u8g2.drawBox(x - 1, y - 1, w + 2, (int)(cur.lid * h) + 1);

  if (cur.happy > 0.02f) {                                    // happy arch
    int rr = (int)(w * 0.6f);
    int cy = (int)((y + h + rr) - cur.happy * (rr + h * 0.15f));
    u8g2.drawDisc(x + w / 2, cy, rr);
    if (cy < y + h) {
      u8g2.drawBox(x - 2, cy, w + 4, y + h - cy + 2);         // erase fangs below the arch
    }
  }

  u8g2.setDrawColor(1);
}

float blinkScale() {
  uint32_t d = millis() - blinkStart;
  if (d >= 180) return 1.0f;
  return 1.0f - 0.92f * sinf(PI * d / 180.0f);
}

void renderFace() {
  int w = (int)cur.w;
  int gap = 14;
  int lx = 64 - gap / 2 - w + (int)lookX;
  int rx = 64 + gap / 2 + (int)lookX;
  bool canBlink = cur.closed < 0.5f && cur.love < 0.5f && cur.dizzy < 0.5f;
  int h = canBlink ? (int)(cur.h * blinkScale()) : (int)cur.h;
  if (h < 2) h = 2;
  int y = 32 - h / 2 + (int)lookY;

  if (cur.closed > 0.5f) {
    // ---- sleeping ----
    u8g2.drawRBox(lx, 32, w, 3, 1);
    u8g2.drawRBox(rx, 32, w, 3, 1);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(100, 20, "z");
    u8g2.drawStr(108, 11, "Z");

  } else if (cur.love > 0.5f) {
    // ---- hearts ----
    int s = 9 + (int)(2 * sinf(millis() / 110.0f));
    drawHeart(lx + w / 2, 32, s);
    drawHeart(rx + w / 2, 32, s);

  } else if (cur.dizzy > 0.5f) {
    // ---- X eyes ----
    int s = 10;
    int lcx = lx + w / 2, lcy = 32;
    int rcx = rx + w / 2, rcy = 32;
    u8g2.drawLine(lcx - s, lcy - s, lcx + s, lcy + s);
    u8g2.drawLine(lcx + s, lcy - s, lcx - s, lcy + s);
    u8g2.drawLine(rcx - s, rcy - s, rcx + s, rcy + s);
    u8g2.drawLine(rcx + s, rcy - s, rcx - s, rcy + s);

  } else {
    // ---- normal / wink / squint / etc. ----
    drawEye(lx, y, w, h, true);
    drawEye(rx, y, w, h, false);
  }

  // ---- headphones overlay ----
  if (musicPlaying || curExpr == E_MUSIC) {
    drawHeadphones(32 + (int)lookY);
  }
}

// ---------------------------------- animation update ----------------------------------
void updateFace(uint32_t now) {
  // ---- time-based tweening (jitter-free) ----
  float dt = (now - prevUpdateTime) / 1000.0f;
  prevUpdateTime = now;
  if (dt > 0.15f) dt = 0.15f;           // cap to prevent jumps after stalls
  if (dt < 0.001f) dt = 0.001f;

  // Original tween factor was 0.28 at 25 fps (40 ms).
  // Time-independent equivalent: alpha = 1 - 0.72^(dt * 25)
  float alpha = 1.0f - powf(0.72f, dt * 25.0f);

  auto tween = [alpha](float& a, float b) { a += (b - a) * alpha; };
  tween(cur.w, tgt.w);         tween(cur.h, tgt.h);
  tween(cur.tilt, tgt.tilt);   tween(cur.happy, tgt.happy);
  tween(cur.lid, tgt.lid);     tween(cur.love, tgt.love);
  tween(cur.closed, tgt.closed);
  tween(cur.wink, tgt.wink);   tween(cur.dizzy, tgt.dizzy);
  tween(cur.squint, tgt.squint);

  // ---- random look-around ----
  if (now > nextLookAt) {
    lookTX = random(-8, 9);
    lookTY = random(-4, 5);
    nextLookAt = now + random(1200, 4000);
  }
  float lookGain = (cur.closed < 0.5f && cur.love < 0.5f && cur.dizzy < 0.5f) ? 1.0f : 0.0f;
  float lookAlpha = 1.0f - powf(0.75f, dt * 25.0f);
  lookX += (lookTX * lookGain - lookX) * lookAlpha;
  lookY += (lookTY * lookGain - lookY) * lookAlpha;

  // ---- blinking ----
  if (now > nextBlinkAt) {
    blinkStart = now;
    nextBlinkAt = now + random(2000, 6000);
  }

  // ---- expression timeout ----
  if (exprUntil && now > exprUntil) {
    if (musicPlaying) setExpr(E_MUSIC);
    else              setExpr(E_NEUTRAL);
  }

  // ---- emotional mood changes ----
  if (cfg.emotional && !exprUntil && !musicPlaying && now > nextMoodAt) {
    static const Expr moods[] = {
      E_HAPPY, E_SLEEPY, E_SURPRISED, E_SAD, E_HAPPY,
      E_ANGRY, E_WINK, E_COOL, E_SHY
    };
    setExpr(moods[random(0, 9)], random(3000, 6000));
    nextMoodAt = now + random(8000, 20000);
  }
}

void render() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  renderFace();
  u8g2.sendBuffer();
}

// ---------------------------------- touch ----------------------------------
void wake() {
  lastTouchAt = millis();
  if (sleeping) { sleeping = false; u8g2.setPowerSave(0); setExpr(E_SURPRISED, 1500); }
}

void onTap() {
  tapIdx = (tapIdx + 1) % E_SLEEP;        // cycle through all expressions except sleeping
  setExpr((Expr)tapIdx, 6000);
}

void onDoubleTap() {
  cfg.emotional = !cfg.emotional;
  saveSettings();
  // visual feedback: surprised when on, sleepy when off
  setExpr(cfg.emotional ? E_SURPRISED : E_SLEEPY, 1500);
}

void onLongPress() {
  setExpr(E_LOVE, 4000);
}

void handleTouch() {
  static bool last = false, longFired = false;
  static uint32_t downAt = 0, lastTapAt = 0;
  static int taps = 0;
  bool t = digitalRead(PIN_TOUCH) == HIGH;
  uint32_t now = millis();

  if (t && !last) { downAt = now; longFired = false; wake(); }
  if (t && !longFired && now - downAt >= 3000) { longFired = true; onLongPress(); }
  if (!t && last && !longFired && now - downAt < 600) { taps++; lastTapAt = now; }
  if (taps && !t && now - lastTapAt > 350) {
    if (taps == 1) onTap(); else onDoubleTap();
    taps = 0;
  }
  last = t;
}

// ---------------------------------- serial music detection ----------------------------------
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'M') {
      bool wasPlaying = musicPlaying;
      musicPlaying = true;
      wake();                                  // keep display awake while listening
      if (!wasPlaying || curExpr != E_MUSIC) {
        if (!exprUntil || curExpr != E_LOVE)   // don't interrupt love from long-press
          setExpr(E_MUSIC);
      }
    } else if (c == 'm') {
      bool wasPlaying = musicPlaying;
      musicPlaying = false;
      if (wasPlaying && curExpr == E_MUSIC)
        setExpr(E_NEUTRAL);
    }
  }
}

// ---------------------------------- Arduino entry points ----------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT_PULLDOWN);
  randomSeed(esp_random());

  Wire.setPins(PIN_SDA, PIN_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);

  loadSettings();

  bootText("Desk Companion", "Starting...");
  delay(1500);

  // Flush any stale serial data from USB enumeration
  while (Serial.available()) Serial.read();

  cur = PRESETS[E_SLEEP];
  tgt = PRESETS[E_NEUTRAL];

  uint32_t now = millis();
  lastTouchAt   = now;
  nextMoodAt    = now + 10000;
  lastFrame     = now;
  prevUpdateTime = now;
  setExpr(E_NEUTRAL);
}

void loop() {
  handleTouch();
  handleSerial();

  uint32_t now = millis();

  if (!sleeping && now - lastTouchAt > 300000UL) {    // 5 min idle → screen off
    sleeping = true;
    u8g2.setPowerSave(1);
  }
  if (sleeping) { delay(20); return; }

  if (now - lastFrame >= FRAME_MS) {
    lastFrame += FRAME_MS;                            // accumulator: prevents timing drift
    if (now - lastFrame > FRAME_MS * 3)               // way behind → catch up
      lastFrame = now;
    updateFace(now);
    render();
  }
}
