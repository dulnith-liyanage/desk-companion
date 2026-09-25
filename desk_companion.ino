/*
  Desk Companion - Premium Offline Edition (Touch Only)
  Features:
  - Non-blocking Animation Engine (Smooth 30fps)
  - Non-blocking Audio Melody System
  - Advanced PWM Haptic Engine (Heartbeats, Breathing, Purring)
  - Tamagotchi Mechanics & Pomodoro Focus Mode
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

const int PIN_SDA   = 8;
const int PIN_SCL   = 9;
const int PIN_TOUCH = 7;
const int PIN_BUZZER = 5;
const int PIN_VIBE   = 10;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences prefs;

// ==============================================================================
// CORE SYSTEMS & CONSTANTS
// ==============================================================================
enum Expr : uint8_t {
  E_NEUTRAL, E_HAPPY, E_SAD, E_ANGRY, E_SLEEPY, E_SURPRISED, E_LOVE,
  E_WINK, E_SKEPTICAL, E_SHY, E_COOL, E_ATTENTIVE, E_MUSIC, E_SLEEP,
  E_DETERMINED, E_PLEADING, E_COUNT
};

struct Face { float w, h, tilt, happy, lid, love, closed, wink, squint; };
const Face PRESETS[E_COUNT] = {
  { 30,  28,    0,     0,     0,     0,    0,    0,    0   },  // neutral
  { 30,  28,    0,     1,     0,     0,    0,    0,    0   },  // happy
  { 30,  26,   -1,     0,     0,     0,    0,    0,    0   },  // sad
  { 30,  26,    1,     0,     0,     0,    0,    0,    0   },  // angry
  { 30,  28,    0,     0,  0.55f,    0,    0,    0,    0   },  // sleepy
  { 36,  36,    0,     0,     0,     0,    0,    0,    0   },  // surprised
  { 30,  28,    0,     0,     0,     1,    0,    0,    0   },  // love
  { 30,  28,    0,  0.5f,     0,     0,    0,    1,    0   },  // wink
  { 32,  28, 0.4f,     0,     0,     0,    0, 0.4f,    0   },  // skeptical
  { 22,  20,    0,     0,     0,     0,    0,    0,    0   },  // shy
  { 30,  14,    0,  0.3f,     0,     0,    0,    0,    1   },  // cool
  { 18,  42,    0,     0,     0,     0,    0,    0,    0   },  // attentive
  { 30,  28,    0,  0.7f,     0,     0,    0,    0,    0   },  // music
  { 30,  28,    0,     0,     0,     0,    1,    0,    0   },  // sleeping
  { 28,  18, 0.8f,     0,     0,     0,    0,    0, 0.5f },  // determined
  { 38,  34,-0.4f,  0.6f,     0,     0,    0,    0,    0   },  // pleading
};

Expr     curExpr = E_NEUTRAL;
uint32_t exprUntil = 0;
Face     cur, tgt;
float    lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
uint32_t nextLookAt = 0, blinkStart = 0, nextBlinkAt = 0;
uint32_t lastFrame = 0, prevUpdateTime = 0;

bool     sleeping = false;
uint32_t lastTouchAt = 0;
const uint32_t FRAME_MS = 33; // Smooth 30 FPS

// ==============================================================================
// PREMIUM AUDIO ENGINE (Non-Blocking)
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
      curMelody = nullptr;
      noTone(PIN_BUZZER);
    } else {
      int f = curMelody[melodyIdx].f;
      int d = curMelody[melodyIdx].d;
      if (f > 0) tone(PIN_BUZZER, f, d);
      else noTone(PIN_BUZZER);
      noteEndTime = now + d + 20; // 20ms articulation gap
      melodyIdx++;
    }
  }
}

// Sound Library
Note sfxFocusOn[]  = {{600, 100}, {800, 100}, {1200, 150}};
Note sfxFocusOff[] = {{1200, 100}, {800, 100}, {600, 150}};
Note sfxWake[]     = {{1000, 80}, {1500, 120}};
Note sfxSleep[]    = {{800, 150}, {600, 200}};
Note sfxPomodoro[] = {{1000, 150}, {0, 50}, {1500, 150}, {0, 50}, {2000, 400}};
Note sfxHum[]      = {{600, 200}, {650, 200}, {800, 400}, {0, 100}, {650, 400}};
Note sfxSneeze[]   = {{2000, 80}, {1500, 100}};
Note sfxPet[]      = {{1500, 50}};
Note sfxLove[]     = {{800, 100}, {1200, 100}, {1600, 200}};

// ==============================================================================
// PREMIUM HAPTIC ENGINE (PWM)
// ==============================================================================
enum VibeState { V_OFF, V_PULSE_SOFT, V_PULSE_HARD, V_PURR, V_HEARTBEAT, V_BREATHE };
VibeState curVibe = V_OFF;
uint32_t vibeUntil = 0;

void setVibe(VibeState v, uint32_t duration = 0) {
  curVibe = v; 
  vibeUntil = duration ? millis() + duration : 0xffffffff;
}

void updateVibe(uint32_t now) {
  if (curVibe != V_OFF && now > vibeUntil) { 
    curVibe = V_OFF; analogWrite(PIN_VIBE, 0); return; 
  }
  
  switch (curVibe) {
    case V_OFF:        analogWrite(PIN_VIBE, 0); break;
    case V_PULSE_SOFT: analogWrite(PIN_VIBE, 150); break;
    case V_PULSE_HARD: analogWrite(PIN_VIBE, 255); break;
    case V_PURR:       analogWrite(PIN_VIBE, 110); break;
    case V_HEARTBEAT: {
      int c = now % 1200;
      if (c < 120 || (c > 250 && c < 370)) analogWrite(PIN_VIBE, 255);
      else analogWrite(PIN_VIBE, 0);
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
// TAMAGOTCHI & FOCUS MECHANICS
// ==============================================================================
bool isFocusMode = false;
uint32_t focusStartTime = 0;
const uint32_t FOCUS_DUR = 25 * 60 * 1000; // 25 Min Pomodoro

float happiness = 100.0f; // 0.0 to 100.0
uint32_t lastIdleEventAt = 0;

// Non-blocking sneeze sequencer
int sneezeState = 0; 
uint32_t sneezeAt = 0;

void setExpr(Expr e, uint32_t holdMs = 0) {
  curExpr = e; tgt = PRESETS[e]; exprUntil = holdMs ? millis() + holdMs : 0;
}

void wake() {
  lastTouchAt = millis();
  if (sleeping) {
    sleeping = false;
    u8g2.setPowerSave(0);
    setExpr(E_SURPRISED, 1500);
    playMelody(sfxWake, 2);
    setVibe(V_PULSE_HARD, 150);
  }
}

// ==============================================================================
// GRAPHICS & ANIMATION
// ==============================================================================
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
    if (h < 3) { u8g2.drawRBox(x, y + h / 2 - 1, w, 3, 1); return; }
  }
  int r = h / 2; if (r > w / 2) r = w / 2; if (r > 10) r = 10;
  
  u8g2.setDrawColor(1);
  if (r > 0) u8g2.drawRBox(x, y, w, h, r);
  else       u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(0);

  if (cur.tilt > 0.05f) {
    int d = (int)(cur.tilt * h * 0.55f);
    if (left) u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d);
    else      u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d);
  } else if (cur.tilt < -0.05f) {
    int d = (int)(-cur.tilt * h * 0.55f);
    if (left) u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d);
    else      u8g2.drawTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d);
  }
  
  if (cur.lid > 0.02f) u8g2.drawBox(x - 1, y - 1, w + 2, (int)(cur.lid * h) + 1);
  
  if (cur.happy > 0.02f) {
    int rr = (int)(w * 0.6f);
    int cy = (int)((y + h + rr) - cur.happy * (rr + h * 0.15f));
    u8g2.drawDisc(x + w / 2, cy, rr);
    if (cy < y + h) u8g2.drawBox(x - 2, cy, w + 4, y + h - cy + 2);
  }
  u8g2.setDrawColor(1);
}

float blinkScale() {
  uint32_t d = millis() - blinkStart;
  if (d >= 180) return 1.0f;
  return 1.0f - 0.92f * sinf(PI * d / 180.0f);
}

void renderFace(uint32_t now) {
  int w = (int)cur.w;
  int gap = 14;
  int lx = 64 - gap / 2 - w + (int)lookX;
  int rx = 64 + gap / 2 + (int)lookX;
  
  // Sneeze physics offset
  if (sneezeState == 1) {
    int shake = (now / 20) % 2 == 0 ? 2 : -2;
    lx += shake; rx += shake;
  }

  bool canBlink = cur.closed < 0.5f && cur.love < 0.5f;
  int h = canBlink ? (int)(cur.h * blinkScale()) : (int)cur.h;
  if (h < 2) h = 2;
  int y = 32 - h / 2 + (int)lookY;

  if (cur.closed > 0.5f) {
    u8g2.drawRBox(lx, 32, w, 3, 1);
    u8g2.drawRBox(rx, 32, w, 3, 1);
    u8g2.setFont(u8g2_font_6x12_tf);
    int zOffset = (int)(2.5f * sinf(millis() / 400.0f));
    u8g2.drawStr(100, 20 + zOffset, "z");
    u8g2.drawStr(108, 11 - zOffset, "Z");
  } else if (cur.love > 0.5f) {
    int s = 9 + (int)(2 * sinf(millis() / 110.0f));
    drawHeart(lx + w / 2, 32, s);
    drawHeart(rx + w / 2, 32, s);
  } else {
    drawEye(lx, y, w, h, true);
    drawEye(rx, y, w, h, false);
  }
  
  // Focus Mode Progress Bar & Icon
  if (isFocusMode) {
     uint32_t elapsed = now - focusStartTime;
     if (elapsed > FOCUS_DUR) elapsed = FOCUS_DUR;
     int barW = (128 * elapsed) / FOCUS_DUR;
     u8g2.drawBox(0, 62, barW, 2);
     u8g2.setFont(u8g2_font_5x7_tf);
     u8g2.drawStr(53, 7, "FOCUS");
  }
}

void updateEngine(uint32_t now) {
  float dt = (now - prevUpdateTime) / 1000.0f;
  prevUpdateTime = now;
  if (dt > 0.15f) dt = 0.15f;
  if (dt < 0.001f) dt = 0.001f;

  float alpha = 1.0f - powf(0.72f, dt * 25.0f);
  auto tween = [alpha](float& a, float b) { a += (b - a) * alpha; };
  tween(cur.w, tgt.w);         tween(cur.h, tgt.h);
  tween(cur.tilt, tgt.tilt);   tween(cur.happy, tgt.happy);
  tween(cur.lid, tgt.lid);     tween(cur.love, tgt.love);
  tween(cur.closed, tgt.closed);
  tween(cur.wink, tgt.wink);   tween(cur.squint, tgt.squint);

  if (now > nextLookAt) {
    lookTX = random(-8, 9);
    lookTY = random(-4, 5);
    nextLookAt = now + random(1200, 4000);
  }
  float lookGain = (cur.closed < 0.5f && cur.love < 0.5f) ? 1.0f : 0.0f;
  float lookAlpha = 1.0f - powf(0.75f, dt * 25.0f);
  lookX += (lookTX * lookGain - lookX) * lookAlpha;
  lookY += (lookTY * lookGain - lookY) * lookAlpha;

  if (now > nextBlinkAt) {
    blinkStart = now;
    nextBlinkAt = now + random(2000, 6000);
  }

  // Focus Pomodoro Timer
  if (isFocusMode) {
    if (now - focusStartTime >= FOCUS_DUR) {
      isFocusMode = false; 
      playMelody(sfxPomodoro, 5);
      setVibe(V_PULSE_HARD, 1000);
      setExpr(E_HAPPY, 5000);
    }
  } else {
    // Happiness Decay
    happiness -= dt * 0.15f; 
    if (happiness < 0) happiness = 0;
  }

  // Idle Auto-Sleep 
  uint32_t idleTime = now - lastTouchAt;
  if (!isFocusMode && idleTime > 300000) { 
    if (!sleeping) {
      sleeping = true;
      setExpr(E_SLEEP);
      playMelody(sfxSleep, 2);
      setVibe(V_BREATHE, 0); // Breathe indefinitely while sleeping
      u8g2.setPowerSave(0);
    } else if (idleTime > 600000) { 
      u8g2.setPowerSave(1);
    }
  }

  // Sneeze Sequencer (Non-blocking)
  if (sneezeState == 1 && now > sneezeAt) {
    sneezeState = 0;
    setExpr(E_SAD, 1500); 
    playMelody(sfxSneeze, 2);
    setVibe(V_PULSE_HARD, 150);
  }

  // Clear hold expression & revert to baseline
  if (exprUntil && now > exprUntil) {
    exprUntil = 0;
    if (isFocusMode) {
      setExpr(E_ATTENTIVE);
    } else if (happiness < 30) {
      setExpr(E_PLEADING); // Wants attention!
    } else {
      setExpr(E_NEUTRAL);
    }
  }

  // Random Idle Events (if awake, idle, and neutral)
  if (!sleeping && !isFocusMode && (!exprUntil || now > exprUntil)) {
    if (now - lastIdleEventAt > 20000) {
      int r = random(100);
      if (r < 5) {
        // Sneeze startup
        setExpr(E_SKEPTICAL, 3000); 
        sneezeState = 1;
        sneezeAt = now + 800; // Build up for 800ms
      } else if (r < 15) {
        setExpr(E_SLEEPY, 3000); // Yawn
      } else if (r < 25) {
        setExpr(E_MUSIC, 4000);  // Hum a tune
        playMelody(sfxHum, 5);
      }
      lastIdleEventAt = now;
    }
  }
}

// ==============================================================================
// TOUCH INTERACTIONS
// ==============================================================================
void onTap() {
  if (isFocusMode) {
    setExpr(E_SKEPTICAL, 1500); // Quick glance, doesn't interrupt focus
    return;
  }
  
  happiness += 20.0f; // Petting!
  if (happiness > 100.0f) happiness = 100.0f;

  static const Expr reactions[] = { E_HAPPY, E_WINK, E_COOL, E_SHY, E_SURPRISED };
  static Expr lastReaction = E_NEUTRAL;
  Expr nextReaction;
  do {
    nextReaction = reactions[random(0, 5)];
  } while (nextReaction == lastReaction);
  
  lastReaction = nextReaction;
  setExpr(nextReaction, 3000); 
  
  playMelody(sfxPet, 1);
  setVibe(V_PURR, 300); // Soft purr on pet
}

void onDoubleTap() {
  isFocusMode = !isFocusMode;
  if (isFocusMode) {
    focusStartTime = millis();
    setExpr(E_DETERMINED, 3000); 
    playMelody(sfxFocusOn, 3);
    setVibe(V_PULSE_HARD, 300); 
  } else {
    setExpr(E_SURPRISED, 1500);
    playMelody(sfxFocusOff, 3);
    setVibe(V_PULSE_SOFT, 150);
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
      if (stableState) { // Rising Edge (Pressed)
        isPressed = true;
        pressTime = now;
        longPressHandled = false;
        wake();
        
        // Haptic click
        setVibe(V_PULSE_SOFT, 40); 
      } else {           // Falling Edge (Released)
        isPressed = false;
        releaseTime = now;
        
        if (isLoving) {
          isLoving = false;
          setExpr(E_NEUTRAL);
          exprUntil = 0; 
          setVibe(V_OFF); // Stop heartbeat
        } else if (!longPressHandled) {
          tapCount++;
        }
      }
    }
  } else {
    stateChangeTime = now;
  }

  // Long press -> hold to love (Disabled in focus mode)
  if (isPressed && !longPressHandled && !isFocusMode && (now - pressTime > 1200)) {
    longPressHandled = true;
    isLoving = true;
    happiness = 100.0f; // Instant max happiness
    setExpr(E_LOVE, 30000); 
    setVibe(V_HEARTBEAT, 30000); 
    playMelody(sfxLove, 3);
  }

  // Process Taps
  if (!isPressed && tapCount > 0 && (now - releaseTime > 350)) {
    if (tapCount == 1) onTap();
    else if (tapCount >= 2) onDoubleTap();
    tapCount = 0;
  }
}

// ==============================================================================
// MAIN
// ==============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TOUCH, INPUT_PULLDOWN);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // High freq PWM for smooth, silent motor control
  analogWriteFrequency(PIN_VIBE, 20000); 
  analogWriteResolution(PIN_VIBE, 8);
  pinMode(PIN_VIBE, OUTPUT);
  analogWrite(PIN_VIBE, 0);
  
  randomSeed(esp_random());
  prefs.begin("deskcomp", false);

  Wire.setPins(PIN_SDA, PIN_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setFont(u8g2_font_6x12_tf);

  // Boot sequence
  u8g2.clearBuffer();
  int x = (128 - u8g2.getUTF8Width("YETI V2.0")) / 2;
  u8g2.drawStr(x, 36, "YETI V2.0");
  u8g2.sendBuffer();
  
  playMelody(sfxWake, 2);
  setVibe(V_PULSE_HARD, 150);
  delay(1000);

  cur = PRESETS[E_SLEEP];
  tgt = PRESETS[E_NEUTRAL];

  uint32_t now = millis();
  lastTouchAt   = now;
  lastIdleEventAt = now;
  lastFrame     = now;
  prevUpdateTime = now;
  setExpr(E_NEUTRAL);
}

void loop() {
  uint32_t now = millis();
  
  handleTouch();
  updateVibe(now);
  updateMelody(now);
  
  if (now - lastFrame >= FRAME_MS) {
    lastFrame += FRAME_MS;
    if (now - lastFrame > FRAME_MS * 3) lastFrame = now;
    
    updateEngine(now);
    
    if (!sleeping || (now - lastTouchAt <= 600000)) {
      u8g2.clearBuffer();
      u8g2.setDrawColor(1);
      renderFace(now);
      u8g2.sendBuffer();
    } else {
      delay(10);
    }
  }
}
