/*
  Desk Companion - Flappy Yeti & Secret Message Edition
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

const int PIN_SDA   = 8;
const int PIN_SCL   = 9;
const int PIN_TOUCH = 21;
const int PIN_BUZZER = 5;
const int PIN_VIBE   = 10;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences prefs;

enum SystemMode { MODE_NORMAL, MODE_MESSAGE, MODE_GAME };
SystemMode curMode = MODE_NORMAL;

// ==============================================================================
// CORE SYSTEMS & CONSTANTS
// ==============================================================================
enum Expr : uint8_t {
  E_NEUTRAL, E_HAPPY, E_SAD, E_ANGRY, E_FRUSTRATED, E_SLEEPY, E_SURPRISED, 
  E_LOVE, E_WINK, E_SKEPTICAL, E_SHY, E_COOL, E_ATTENTIVE, E_MUSIC, 
  E_SLEEP, E_DETERMINED, E_PLEADING, E_CURIOUS, E_EXCITED, E_COUNT
};

struct Face { float w, h, tilt, happy, lid, love, closed, wink, squint, asym; };
const Face PRESETS[E_COUNT] = {
  { 30,  28,    0,     0,     0,     0,    0,    0,    0,    0 },  // neutral (Classic)
  { 34,  32,    0,  0.8f,     0,     0,    0,    0,    0,    0 },  // happy 
  { 34,  28,-0.4f,     0,     0,     0,    0,    0,    0,    0 },  // sad 
  { 32,  24, 0.6f,     0,  0.3f,     0,    0,    0,    0,    0 },  // angry 
  { 38,  16, 0.8f,     0,     0,     0,    0,    0, 0.5f,    0 },  // frustrated 
  { 34,  30,    0,     0,  0.6f,     0,    0,    0,    0,    0 },  // sleepy
  { 40,  38,    0,     0,     0,     0,    0,    0,    0,    0 },  // surprised
  { 34,  30,    0,     0,     0,  1.0f,    0,    0,    0,    0 },  // love
  { 34,  30,    0,  0.5f,     0,     0,    0,  1.0f,   0,    0 },  // wink
  { 32,  24, 0.3f,     0,     0,     0,    0,    0, 0.5f,    0 },  // skeptical
  { 22,  20,    0,     0,     0,     0,    0,    0,    0,    0 },  // shy
  { 32,  14,    0,  0.2f,     0,     0,    0,    0, 0.8f,    0 },  // cool
  { 22,  40,    0,     0,     0,     0,    0,    0,    0,    0 },  // attentive
  { 34,  30,    0,  0.7f,     0,     0,    0,    0,    0,    0 },  // music
  { 34,  30,    0,     0,     0,     0,  1.0f,   0,    0,    0 },  // sleeping
  { 30,  20, 0.6f,     0,     0,     0,    0,    0, 0.3f,    0 },  // determined
  { 38,  34,-0.3f,  0.5f,     0,     0,    0,    0,    0,    0 },  // pleading
  { 34,  34,    0,     0,     0,     0,    0,    0,    0, 0.35f},  // curious 
  { 38,  38,    0,  0.4f,     0,     0,    0,    0,    0,    0 },  // excited 
};

Expr     curExpr = E_NEUTRAL;
uint32_t exprUntil = 0;
Face     cur, tgt;
float    lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
uint32_t nextLookAt = 0, blinkStart = 0, nextBlinkAt = 0;
uint32_t lastFrame = 0, prevUpdateTime = 0;

bool     sleeping = false;
uint32_t lastTouchAt = 0;
const uint32_t FRAME_MS = 33; 

// ==============================================================================
// PREMIUM AUDIO ENGINE
// ==============================================================================
struct Note { int f; int d; };
Note* curMelody = nullptr;
int melodyLen = 0, melodyIdx = 0;
uint32_t noteEndTime = 0;

void playMelody(Note* m, int len) {
  curMelody = m; melodyLen = len; melodyIdx = 0; noteEndTime = 0;
}

void updateMelody(uint32_t now) {
  if (curMelody && now >= noteEndTime) {
    if (melodyIdx >= melodyLen) {
      curMelody = nullptr; noTone(PIN_BUZZER);
    } else {
      int f = curMelody[melodyIdx].f;
      int d = curMelody[melodyIdx].d;
      if (f > 0) tone(PIN_BUZZER, f, d); else noTone(PIN_BUZZER);
      noteEndTime = now + d + 20; melodyIdx++;
    }
  }
}

Note sfxFocusOn[]  = {{600, 100}, {800, 100}, {1200, 150}};
Note sfxFocusOff[] = {{1200, 100}, {800, 100}, {600, 150}};
Note sfxWake[]     = {{1000, 80}, {1500, 120}};
Note sfxSleep[]    = {{800, 150}, {600, 200}};
Note sfxPomodoro[] = {{1000, 150}, {0, 50}, {1500, 150}, {0, 50}, {2000, 400}};
Note sfxHum[]      = {{600, 200}, {650, 200}, {800, 400}, {0, 100}, {650, 400}};
Note sfxSneeze[]   = {{2000, 80}, {1500, 100}};
Note sfxPet[]      = {{1500, 50}};
Note sfxLove[]     = {{800, 100}, {1200, 100}, {1600, 200}};
Note sfxPoint[]    = {{1200, 50}, {1800, 80}};

// ==============================================================================
// PREMIUM HAPTIC ENGINE
// ==============================================================================
enum VibeState { V_OFF, V_PULSE_SOFT, V_PULSE_HARD, V_PURR, V_HEARTBEAT, V_BREATHE };
VibeState curVibe = V_OFF;
uint32_t vibeUntil = 0;

void setVibe(VibeState v, uint32_t duration = 0) {
  curVibe = v; vibeUntil = duration ? millis() + duration : 0xffffffff;
}

void updateVibe(uint32_t now) {
  if (curVibe != V_OFF && now > vibeUntil) { curVibe = V_OFF; analogWrite(PIN_VIBE, 0); return; }
  switch (curVibe) {
    case V_OFF:        analogWrite(PIN_VIBE, 0); break;
    case V_PULSE_SOFT: analogWrite(PIN_VIBE, 150); break;
    case V_PULSE_HARD: analogWrite(PIN_VIBE, 255); break;
    case V_PURR:       analogWrite(PIN_VIBE, 110); break;
    case V_HEARTBEAT: {
      int c = now % 1200;
      if (c < 120 || (c > 250 && c < 370)) analogWrite(PIN_VIBE, 255); else analogWrite(PIN_VIBE, 0);
      break;
    }
    case V_BREATHE: {
      float w = (sin(now / 600.0f) + 1.0f) / 2.0f;
      analogWrite(PIN_VIBE, (int)(w * 150));
      break;
    }
  }
}

// ==============================================================================
// TAMAGOTCHI & NORMAL GRAPHICS
// ==============================================================================
bool isFocusMode = false;
uint32_t focusStartTime = 0;
const uint32_t FOCUS_DUR = 25 * 60 * 1000;

float happiness = 100.0f;
uint32_t lastIdleEventAt = 0;
int sneezeState = 0; uint32_t sneezeAt = 0;

void setExpr(Expr e, uint32_t holdMs = 0) {
  curExpr = e; tgt = PRESETS[e]; exprUntil = holdMs ? millis() + holdMs : 0;
}

void wake() {
  lastTouchAt = millis();
  if (sleeping) {
    sleeping = false; u8g2.setPowerSave(0);
    setExpr(E_SURPRISED, 1500); playMelody(sfxWake, 2); setVibe(V_PULSE_HARD, 150);
  }
}

void drawHeart(int cx, int cy, int s) {
  u8g2.drawDisc(cx - s / 2, cy - s / 3, s / 2 + 1);
  u8g2.drawDisc(cx + s / 2, cy - s / 3, s / 2 + 1);
  u8g2.drawTriangle(cx - s - 1, cy - s / 3 + 2, cx + s + 1, cy - s / 3 + 2, cx, cy + s);
}

void drawEye(int x, int y, int w, int h, bool left) {
  if (cur.squint > 0.02f) {
    int sq = (int)(cur.squint * h * 0.5f); h -= sq; y += sq / 2;
    if (h < 4) h = 4;
  }
  if (left && cur.wink > 0.02f) {
    int wr = (int)(cur.wink * h * 0.9f); h -= wr; y += wr / 2;
    if (h < 4) { u8g2.drawRBox(x, y + h / 2 - 2, w, 4, 2); return; }
  }
  
  int r = h / 2; if (r > w / 2) r = w / 2; if (r > 10) r = 10;
  
  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, y, w, h, r);
  u8g2.setDrawColor(0); 

  if (cur.tilt > 0.05f) {
    int d = (int)(cur.tilt * h * 0.65f);
    if (left) u8g2.drawTriangle(x - 4, y - 4, x + w + 4, y - 4, x + w + 4, y + d);
    else      u8g2.drawTriangle(x - 4, y - 4, x + w + 4, y - 4, x - 4, y + d);
  } else if (cur.tilt < -0.05f) {
    int d = (int)(-cur.tilt * h * 0.65f);
    if (left) u8g2.drawTriangle(x - 4, y - 4, x + w + 4, y - 4, x - 4, y + d);
    else      u8g2.drawTriangle(x - 4, y - 4, x + w + 4, y - 4, x + w + 4, y + d);
  }
  
  if (cur.lid > 0.02f) u8g2.drawBox(x - 4, y - 4, w + 8, (int)(cur.lid * h) + 4);
  
  if (cur.happy > 0.02f) {
    int rr = (int)(w * 0.65f); 
    int cy = (int)((y + h + rr) - cur.happy * (rr + h * 0.25f)); 
    u8g2.drawDisc(x + w / 2, cy, rr);
    if (cy < y + h) u8g2.drawBox(x - 4, cy, w + 8, y + h - cy + 4);
  }
  u8g2.setDrawColor(1);
}

float blinkScale() {
  uint32_t d = millis() - blinkStart;
  if (d >= 180) return 1.0f;
  return 1.0f - 0.92f * sinf(PI * d / 180.0f);
}

void renderNormal(uint32_t now) {
  int wL = (int)(cur.w * (1.0f + cur.asym));
  int wR = (int)(cur.w * (1.0f - cur.asym));
  int hL = (int)(cur.h * (1.0f + cur.asym));
  int hR = (int)(cur.h * (1.0f - cur.asym));

  int gap = 14; 
  int lx = 64 - gap / 2 - wL + (int)lookX;
  int rx = 64 + gap / 2 + (int)lookX;
  
  if (sneezeState == 1 || curExpr == E_FRUSTRATED) {
    int shake = (now / 20) % 2 == 0 ? 3 : -3;
    lx += shake; rx += shake;
  }

  bool canBlink = cur.closed < 0.5f && cur.love < 0.5f;
  float bScale = canBlink ? blinkScale() : 1.0f;
  
  int curHL = (int)(hL * bScale); if (curHL < 3) curHL = 3;
  int curHR = (int)(hR * bScale); if (curHR < 3) curHR = 3;

  int yL = 32 - curHL / 2 + (int)lookY;
  int yR = 32 - curHR / 2 + (int)lookY;

  if (curExpr == E_EXCITED) {
     int bounce = (int)(abs(sin(now / 80.0f)) * 6.0f);
     yL -= bounce; yR -= bounce;
  }

  if (cur.closed > 0.5f) {
    u8g2.drawRBox(lx, 32 + (int)lookY, wL, 4, 2);
    u8g2.drawRBox(rx, 32 + (int)lookY, wR, 4, 2);
    
    u8g2.setFont(u8g2_font_6x12_tf);
    int z1 = (int)(2.5f * sinf(now / 300.0f));
    int z2 = (int)(2.5f * sinf((now+300) / 300.0f));
    int z3 = (int)(2.5f * sinf((now+600) / 300.0f));
    u8g2.drawStr(95, 28 + z1, "z");
    u8g2.drawStr(105, 18 + z2, "Z");
    u8g2.drawStr(115,  8 + z3, "z");
  } else if (cur.love > 0.5f) {
    int s = 18 + (int)(3 * sinf(now / 110.0f));
    drawHeart(lx + wL / 2, 32 + (int)lookY, s);
    drawHeart(rx + wR / 2, 32 + (int)lookY, s);
  } else {
    drawEye(lx, yL, wL, curHL, true);
    drawEye(rx, yR, wR, curHR, false);
  }
  
  if (isFocusMode) {
     uint32_t elapsed = now - focusStartTime;
     if (elapsed > FOCUS_DUR) elapsed = FOCUS_DUR;
     int barW = (128 * elapsed) / FOCUS_DUR;
     u8g2.drawBox(0, 62, barW, 2);
     u8g2.setFont(u8g2_font_5x7_tf);
     u8g2.drawStr(53, 7, "FOCUS");
  }
}

void updateNormal(uint32_t now) {
  float dt = (now - prevUpdateTime) / 1000.0f;
  prevUpdateTime = now;
  if (dt > 0.15f) dt = 0.15f; if (dt < 0.001f) dt = 0.001f;

  float alpha = 1.0f - powf(0.72f, dt * 25.0f);
  auto tween = [alpha](float& a, float b) { a += (b - a) * alpha; };
  tween(cur.w, tgt.w);         tween(cur.h, tgt.h);
  tween(cur.tilt, tgt.tilt);   tween(cur.happy, tgt.happy);
  tween(cur.lid, tgt.lid);     tween(cur.love, tgt.love);
  tween(cur.closed, tgt.closed);
  tween(cur.wink, tgt.wink);   tween(cur.squint, tgt.squint);
  tween(cur.asym, tgt.asym);

  if (now > nextLookAt) {
    lookTX = random(-6, 7); lookTY = random(-4, 5);
    nextLookAt = now + random(1200, 4000);
  }
  float lookGain = (cur.closed < 0.5f && cur.love < 0.5f) ? 1.0f : 0.0f;
  float lookAlpha = 1.0f - powf(0.75f, dt * 25.0f);
  lookX += (lookTX * lookGain - lookX) * lookAlpha;
  lookY += (lookTY * lookGain - lookY) * lookAlpha;

  if (now > nextBlinkAt) {
    blinkStart = now; nextBlinkAt = now + random(2000, 6000);
  }

  if (isFocusMode) {
    if (now - focusStartTime >= FOCUS_DUR) {
      isFocusMode = false; playMelody(sfxPomodoro, 5); setVibe(V_PULSE_HARD, 1000); setExpr(E_HAPPY, 5000);
    }
  } else {
    happiness -= dt * 0.15f; if (happiness < 0) happiness = 0;
  }

  uint32_t idleTime = now - lastTouchAt;
  if (!isFocusMode && idleTime > 300000) { 
    if (!sleeping) {
      sleeping = true; setExpr(E_SLEEP); playMelody(sfxSleep, 2); setVibe(V_BREATHE, 0); u8g2.setPowerSave(0);
    } else if (idleTime > 600000) { 
      u8g2.setPowerSave(1);
    }
  }

  if (sneezeState == 1 && now > sneezeAt) {
    sneezeState = 0; setExpr(E_SAD, 1500); playMelody(sfxSneeze, 2); setVibe(V_PULSE_HARD, 150);
  }

  if (exprUntil && now > exprUntil) {
    exprUntil = 0;
    if (isFocusMode) setExpr(E_ATTENTIVE);
    else if (happiness < 30) setExpr(E_PLEADING);
    else setExpr(E_NEUTRAL);
  }

  if (!sleeping && !isFocusMode && (!exprUntil || now > exprUntil)) {
    if (now - lastIdleEventAt > 15000) {
      int r = random(100);
      if (r < 5) {
        setExpr(E_SKEPTICAL, 3000); sneezeState = 1; sneezeAt = now + 800;
      } else if (r < 15) {
        setExpr(E_SLEEPY, 3000); 
      } else if (r < 25) {
        setExpr(E_MUSIC, 4000); playMelody(sfxHum, 5);
      } else if (r < 30) {
        setExpr(E_CURIOUS, 3000); 
      }
      lastIdleEventAt = now;
    }
  }
}

// ==============================================================================
// MESSAGE MODE
// ==============================================================================
uint32_t msgStart = 0;
bool lovePlayed = false;

void onTripleTap() {
    curMode = MODE_MESSAGE;
    msgStart = millis();
    lovePlayed = false;
    setVibe(V_PULSE_HARD, 100);
}

void updateMessage(uint32_t now) {
    uint32_t t = now - msgStart;
    if (t > 8000) {
        curMode = MODE_NORMAL;
        setExpr(E_NEUTRAL);
    } else if (t > 5000 && !lovePlayed) {
        lovePlayed = true;
        setVibe(V_HEARTBEAT, 3000);
        playMelody(sfxLove, 3);
    }
}

void renderMessage(uint32_t now) {
    uint32_t t = now - msgStart;
    if (t < 1500) {
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(28, 35, "SYSTEM ERROR");
    } else if (t < 3000) {
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(8, 35, "HEART.EXE CORRUPT");
    } else if (t < 4000) {
        for(int i=0; i<400; i++) u8g2.drawPixel(random(128), random(64));
    } else if (t < 5000) {
        // silence
    } else {
        u8g2.setFont(u8g2_font_8x13B_tf);
        u8g2.drawStr(22, 25, "I LOVE YOU!");
        int bounce = (int)(abs(sin(now / 150.0f)) * 5.0f);
        drawHeart(64, 50 - bounce, 16); 
    }
}

// ==============================================================================
// GAME MODE (FLAPPY YETI)
// ==============================================================================
float gy = 32, gv = 0, px = 128, pg = 32;
int gscore = 0, gstate = 0; 

void onQuadTap() {
    curMode = MODE_GAME;
    gstate = 0;
    setVibe(V_PULSE_SOFT, 100);
}

void onGamePress() {
    if (gstate == 0) {
        gstate = 1; gy = 32; gv = -3.5f; px = 128; gscore = 0; pg = random(20, 44);
        playMelody(sfxPet, 1);
    } else if (gstate == 1) {
        gv = -3.5f; playMelody(sfxPet, 1); setVibe(V_PULSE_SOFT, 30);
    } else if (gstate == 2) {
        gstate = 0; 
    }
}

void dieGame() {
    gstate = 2; playMelody(sfxSneeze, 2); setVibe(V_PULSE_HARD, 300);
}

void updateGame(uint32_t now) {
    if (gstate == 1) {
        gv += 0.35f; gy += gv; px -= 2.8f; 
        
        if (px < -15) {
            px = 128; pg = random(20, 44);
            gscore++; playMelody(sfxPoint, 2); 
        }
        
        if (gy > 58 || gy < -5) dieGame();
        if (px < 34 && px + 12 > 24) { 
            if (gy < pg - 14 || gy + 10 > pg + 14) dieGame();
        }
    }
}

void renderGame(uint32_t now) {
    u8g2.setFont(u8g2_font_6x12_tf);
    if (gstate == 0) {
        u8g2.drawStr(28, 20, "FLAPPY YETI");
        u8g2.drawStr(28, 40, "Tap to Jump");
        u8g2.drawStr(12, 55, "(Long Hold to Exit)");
    } else if (gstate == 1) {
        // Cute mini yeti
        u8g2.drawRBox(24, (int)gy, 10, 10, 2);
        u8g2.setDrawColor(0);
        u8g2.drawBox(26, (int)gy+2, 2, 3); u8g2.drawBox(30, (int)gy+2, 2, 3);
        u8g2.setDrawColor(1);
        
        // Pipes
        u8g2.drawBox((int)px, 0, 12, (int)pg - 14);
        u8g2.drawBox((int)px, (int)pg + 14, 12, 64);
        
        u8g2.setCursor(2, 10); u8g2.print(gscore);
    } else {
        u8g2.drawStr(35, 30, "GAME OVER");
        u8g2.setCursor(40, 45); u8g2.print("Score: "); u8g2.print(gscore);
    }
}

// ==============================================================================
// TOUCH LOGIC
// ==============================================================================
void onTap() {
  happiness += 20.0f; if (happiness > 100.0f) happiness = 100.0f;
  static const Expr reactions[] = { E_HAPPY, E_WINK, E_COOL, E_SHY, E_SURPRISED, E_ANGRY, E_FRUSTRATED, E_CURIOUS, E_EXCITED };
  static Expr lastReaction = E_NEUTRAL; Expr nextReaction;
  do { nextReaction = reactions[random(0, 9)]; } while (nextReaction == lastReaction);
  lastReaction = nextReaction; setExpr(nextReaction, 3000); 
  playMelody(sfxPet, 1); setVibe(V_PURR, 300);
}

void onDoubleTap() {
  isFocusMode = !isFocusMode;
  if (isFocusMode) {
    focusStartTime = millis(); setExpr(E_DETERMINED, 3000); playMelody(sfxFocusOn, 3); setVibe(V_PULSE_HARD, 300); 
  } else {
    setExpr(E_SURPRISED, 1500); playMelody(sfxFocusOff, 3); setVibe(V_PULSE_SOFT, 150);
  }
}

void handleTouch() {
  static uint32_t pressTime = 0, releaseTime = 0;
  static bool isPressed = false;
  static int tapCount = 0;
  static bool longPressHandled = false;
  static bool isLoving = false; 
  static bool stableState = false;
  static uint32_t stateChangeTime = 0;
  
  bool rawState = digitalRead(PIN_TOUCH) == HIGH;
  uint32_t now = millis();

  if (rawState != stableState) {
    if (now - stateChangeTime > 20) { 
      stableState = rawState;
      if (stableState) {
        isPressed = true; pressTime = now; longPressHandled = false; 
        if (curMode == MODE_GAME) {
            onGamePress();
        } else {
            wake(); setVibe(V_PULSE_SOFT, 40); 
        }
      } else {
        isPressed = false; releaseTime = now;
        if (curMode != MODE_GAME) {
            if (isLoving) {
              isLoving = false; setExpr(E_NEUTRAL); exprUntil = 0; setVibe(V_OFF);
            } else if (!longPressHandled) {
              tapCount++;
            }
        }
      }
    }
  } else {
    stateChangeTime = now;
  }

  // Tap evaluation for Normal / Message modes
  if (curMode != MODE_GAME && !isPressed && tapCount > 0 && (now - releaseTime > 400)) {
    if (tapCount == 1) onTap();
    else if (tapCount == 2) onDoubleTap();
    else if (tapCount == 3) onTripleTap();
    else if (tapCount >= 4) onQuadTap();
    tapCount = 0;
  }

  // Long press handling
  if (isPressed && !longPressHandled && (now - pressTime > 1200)) {
    if (curMode == MODE_GAME) {
       longPressHandled = true; curMode = MODE_NORMAL; playMelody(sfxFocusOff, 3); setVibe(V_PULSE_HARD, 200);
    } else if (curMode == MODE_NORMAL && !isFocusMode) {
       longPressHandled = true; isLoving = true; happiness = 100.0f;
       setExpr(E_LOVE, 30000); setVibe(V_HEARTBEAT, 30000); playMelody(sfxLove, 3);
    }
  }
}

void setup() {
  Serial.begin(115200); pinMode(PIN_TOUCH, INPUT_PULLDOWN); pinMode(PIN_BUZZER, OUTPUT);
  analogWriteFrequency(PIN_VIBE, 20000); analogWriteResolution(PIN_VIBE, 8); pinMode(PIN_VIBE, OUTPUT); analogWrite(PIN_VIBE, 0);
  randomSeed(esp_random()); prefs.begin("deskcomp", false);
  Wire.setPins(PIN_SDA, PIN_SCL); u8g2.begin(); u8g2.setBusClock(400000); u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.clearBuffer(); int x = (128 - u8g2.getUTF8Width("YETI V2.0")) / 2; u8g2.drawStr(x, 36, "YETI V2.0"); u8g2.sendBuffer();
  playMelody(sfxWake, 2); setVibe(V_PULSE_HARD, 150); delay(1000);

  cur = PRESETS[E_SLEEP]; tgt = PRESETS[E_NEUTRAL];
  uint32_t now = millis(); lastTouchAt = now; lastIdleEventAt = now; lastFrame = now; prevUpdateTime = now;
  setExpr(E_NEUTRAL);
}

void loop() {
  uint32_t now = millis();
  handleTouch(); updateVibe(now); updateMelody(now);
  
  if (now - lastFrame >= FRAME_MS) {
    lastFrame += FRAME_MS; if (now - lastFrame > FRAME_MS * 3) lastFrame = now;
    
    if (curMode == MODE_GAME) updateGame(now);
    else if (curMode == MODE_MESSAGE) updateMessage(now);
    else updateNormal(now);

    if (!sleeping || (now - lastTouchAt <= 600000) || curMode != MODE_NORMAL) {
      u8g2.clearBuffer(); u8g2.setDrawColor(1); 
      
      if (curMode == MODE_GAME) renderGame(now);
      else if (curMode == MODE_MESSAGE) renderMessage(now);
      else renderNormal(now);
      
      u8g2.sendBuffer();
    } else { delay(10); }
  }
}
