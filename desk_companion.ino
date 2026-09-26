/*
  Desk Companion - Polished Idle, Yeti Run & Better Focus
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>

WebServer server(80);
const long gmtOffset_sec = 19800; // UTC+5:30
const int daylightOffset_sec = 0;

int pomoWorkMins = 25;
int pomoBreakMins = 5;
// Alarm State
bool hideAlarm = false;
int al1H = 7, al1M = 0; bool al1En = false;
int al2H = 8, al2M = 0; bool al2En = false;
int al3H = 9, al3M = 0; bool al3En = false;

// Weather State
String weatherCity = "London";
String weatherUnits = "metric";
int weatherTheme = 1;
char curWeatherDesc[32] = "Loading...";
float curTemp = 0.0;
int curHumidity = 0;
float curWind = 0.0;
int alarmManagerSel = 0;
int curWeatherId = 800;
bool forceWeatherFetch = true;
int clockDisplayState = 0;

int pomoTheme = 0;
int clockTheme = 0;
String alMsg = "WAKE UP!";
int alSound = 0;
bool alarmTriggered = false;
int lastAlarmCheckMinute = -1;

String localIP = "";

#include <WiFi.h>

// --- WIFI CONFIGURATION ---
// IMPORTANT: Replace these with your actual 2.4GHz WiFi credentials!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

struct Note { int f; int d; };
Note sfxBeat[] = { {150, 15}, {0, 0} };
bool wifiConnected = false;
bool wifiFailed = false;
uint32_t lastWiFiAttempt = 0;


const int PIN_SDA   = 8;
const int PIN_SCL   = 9;
const int PIN_TOUCH = 3;
const int PIN_BUZZER = 5;
const int PIN_VIBE   = 10;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences prefs;

enum SystemMode { MODE_NORMAL, MODE_GAME, MODE_CLOCK, MODE_ALARM };
SystemMode curMode = MODE_NORMAL;


void updateWiFi(uint32_t now) { /* Now handled in setup */ }


// ==============================================================================
// CORE SYSTEMS & CONSTANTS
// ==============================================================================

enum Expr : uint8_t {
  E_NEUTRAL, E_HAPPY, E_SAD, E_ANGRY, E_FRUSTRATED, E_SLEEPY, E_SURPRISED, 
  E_LOVE, E_WINK, E_SKEPTICAL, E_SHY, E_COOL, E_ATTENTIVE, E_MUSIC, 
  E_SLEEP, E_DETERMINED, E_PLEADING, E_CURIOUS, E_EXCITED, E_FIRE, E_COUNT
};

struct Face { float w, h, tilt, happy, lid, love, closed, wink, squint, asym; };
const Face PRESETS[E_COUNT] = {
  { 30,  28,    0,     0,     0,     0,    0,    0,    0,    0 },  // neutral
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
  { 30,  28,    0,     0,     0,     0,    0,    0,    0,    0 },  // fire
};

Expr     curExpr = E_NEUTRAL;
uint32_t exprUntil = 0;
Face     cur, tgt;
Expr currentExpr = E_NEUTRAL;
float    lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
uint32_t nextLookAt = 0, blinkStart = 0, nextBlinkAt = 0;
uint32_t lastFrame = 0, prevUpdateTime = 0;

bool     sleeping = false;
uint32_t lastTouchAt = 0;
const uint32_t FRAME_MS = 33; 

// ==============================================================================
// PREMIUM AUDIO ENGINE
// ==============================================================================
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
Note sfxChime[] = { {440, 200}, {554, 200}, {659, 400}, {0, 0} };
Note sfxSiren[] = { {880, 100}, {1108, 100}, {880, 100}, {1108, 100}, {0, 0} };
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
uint32_t focusDur = 25 * 60 * 1000;
bool isFocusBreak = false;

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


void drawFireEye(int x, int y, int w, int h, bool isLeft, uint32_t now) {
    u8g2.drawDisc(x, y + h/2 - 6, w/2);
    int yOff1 = sin(now / 150.0 + (isLeft?0:1)) * 3;
    int yOff2 = cos(now / 120.0 + (isLeft?0:1)) * 4;
    int yOff3 = sin(now / 100.0 + (isLeft?1:0)) * 2;
    u8g2.drawTriangle(x - w/2, y + h/2 - 6, x - w/4, y - h/2 + yOff1, x, y + h/2 - 6);
    u8g2.drawTriangle(x - w/4, y + h/2 - 6, x + 2, y - h/2 - 6 + yOff2, x + w/4, y + h/2 - 6);
    u8g2.drawTriangle(x, y + h/2 - 6, x + w/2 - 2, y - h/2 + 2 + yOff3, x + w/2, y + h/2 - 6);
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
    // REDUCED heart size to be much cuter
    int s = 14 + (int)(2 * sinf(now / 120.0f));
    drawHeart(lx + wL / 2, 32 + (int)lookY, s);
    drawHeart(rx + wR / 2, 32 + (int)lookY, s);
  } else if (curExpr == E_FIRE) {
    drawFireEye(lx, yL, wL, curHL, true, now);
    drawFireEye(rx, yR, wR, curHR, false, now);
  } else {
    drawEye(lx, yL, wL, curHL, true);
    drawEye(rx, yR, wR, curHR, false);
  }
  
  if (isFocusMode && pomoTheme == 1) {
     uint32_t elapsed = now - focusStartTime;
     if (elapsed > focusDur) elapsed = focusDur;
     long secsLeft = (focusDur - elapsed) / 1000;
     char buf[16];
     sprintf(buf, "%02d:%02d LEFT", (int)(secsLeft/60), (int)(secsLeft%60));
     u8g2.setFont(u8g2_font_5x7_tf);
     int w = u8g2.getUTF8Width(buf);
     u8g2.drawStr((128-w)/2, 10, buf);
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
    if (now - focusStartTime >= focusDur) {
      if (!isFocusBreak && pomoBreakMins > 0) {
        isFocusBreak = true;
        focusStartTime = now;
        focusDur = pomoBreakMins * 60000;
        playMelody(sfxPomodoro, 5); setVibe(V_PULSE_HARD, 500); setExpr(E_HAPPY, 3000);
      } else {
        isFocusMode = false; isFocusBreak = false;
        playMelody(sfxWake, 3); setVibe(V_PULSE_HARD, 1000); setExpr(E_EXCITED, 5000);
      }
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
    if (isFocusMode) setExpr(E_DETERMINED);
    else if (happiness < 30) setExpr(E_PLEADING);
    else setExpr(E_NEUTRAL);
  }

  // DYNAMIC IDLE STATE (Now triggers much more frequently)
  if (!sleeping && !isFocusMode && (!exprUntil || now > exprUntil)) {
    if (now - lastIdleEventAt > 12000) { // Every 12 seconds
      int r = random(100);
      if (r < 25) {
        setExpr(E_SKEPTICAL, 3000); sneezeState = 1; sneezeAt = now + 800; // Sneeze more common
      } else if (r < 45) {
        setExpr(E_SLEEPY, 4000); 
      } else if (r < 65) {
        setExpr(E_MUSIC, 4000); playMelody(sfxHum, 5);
      } else if (r < 85) {
        setExpr(E_CURIOUS, 3000); 
      } else {
        setExpr(E_SAD, 3000); // Added lonely/sad to idle
      }
      lastIdleEventAt = now;
    }
  }
}

// Triple tap: toggle Pomodoro focus mode
void onTripleTap() {
  isFocusMode = !isFocusMode;
  isFocusBreak = false;
  if (isFocusMode) {
    focusStartTime = millis(); 
    focusDur = pomoWorkMins * 60000;
    setExpr(E_DETERMINED, 3000); playMelody(sfxFocusOn, 3); setVibe(V_PULSE_HARD, 300); 
  } else {
    setExpr(E_SURPRISED, 1500); playMelody(sfxFocusOff, 3); setVibe(V_PULSE_SOFT, 150);
  }
}

// ==============================================================================
// GAME MODE (YETI RUN - Easier Dino Jump Game)
// ==============================================================================
bool gameButtonState = false;
uint32_t gamePressTime = 0;
bool gDucking = false;
int gobjType = 0;
float gy = 44, gv = 0, px = 128;
int gscore = 0, gstate = 0; 

void onQuadTap() {
    curMode = MODE_GAME; gstate = 0; setVibe(V_PULSE_SOFT, 100);
}

void onGamePress() {
    if (gstate == 0) {
        gstate = 1; gy = 44; gv = 0; px = 128; gscore = 0; gobjType = 0; playMelody(sfxPet, 1);
    } else if (gstate == 1) {
        if (gy >= 44) { 
            gv = -6.5f; playMelody(sfxPet, 1); setVibe(V_PULSE_SOFT, 30);
        }
    } else if (gstate == 2) {
        gstate = 0; 
    }
}

void dieGame() {
    gstate = 2; playMelody(sfxSneeze, 2); setVibe(V_PULSE_HARD, 300);
}

void updateGame(uint32_t now) {
    if (gstate == 1) {
        gv += 0.45f; gy += gv; 
        if (gy > 44) { gy = 44; gv = 0; }
        
        px -= 3.5f + (gscore * 0.1f);
        if (px < -15) { 
            px = 128; gscore++; playMelody(sfxPoint, 2); 
            gobjType = random(0, 3); 
        }
        
        int pTop = gy;
        int pBot = gy + 12;
        int pL = 24;
        int pR = 36;

        int oL = px;
        int oR = px + (gobjType == 0 ? 10 : 14);
        int oTop = 0, oBot = 0;
        if (gobjType == 0) { // Cactus
            oTop = 44; oBot = 56;
        } else if (gobjType == 1) { // Low bird
            oTop = 42; oBot = 50;
        } else if (gobjType == 2) { // High bird
            oTop = 26; oBot = 36;
        }
        
        if (pR > oL && pL < oR && pBot > oTop && pTop < oBot) {
            // Shrink hitbox slightly to be forgiving
            if (pR-2 > oL+2 && pL+2 < oR-2 && pBot-2 > oTop+2 && pTop+2 < oBot-2) dieGame();
        }
    }
}
void renderGame(uint32_t now) {
    u8g2.setFont(u8g2_font_6x12_tf);
    if (gstate == 0) {
        u8g2.drawStr(30, 20, "YETI RUN");
        u8g2.drawStr(25, 42, "Tap to Jump");
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(12, 60, "(Long Hold to Exit)");
    } else if (gstate == 1) {
        u8g2.drawHLine(0, 56, 128);
        
        u8g2.drawRBox(24, (int)gy, 12, 12, 2);
        u8g2.setDrawColor(0); u8g2.drawBox(27, (int)gy+3, 2, 3); u8g2.drawBox(31, (int)gy+3, 2, 3); u8g2.setDrawColor(1);
        
        if (gobjType == 0) {
            u8g2.drawBox((int)px, 44, 10, 12);
            u8g2.drawBox((int)px-3, 48, 3, 4);
            u8g2.drawBox((int)px+10, 46, 3, 4);
        } else {
            int baseY = (gobjType == 1) ? 42 : 26;
            int wingY = ((now / 150) % 2 == 0) ? (baseY - 4) : (baseY + 6);
            u8g2.drawBox((int)px, baseY, 14, 6);
            u8g2.drawTriangle((int)px+4, baseY+2, (int)px+10, baseY+2, (int)px+7, wingY);
        }
        
        u8g2.setCursor(2, 10); u8g2.print(gscore);
    } else {
        u8g2.drawStr(35, 30, "GAME OVER");
        u8g2.setCursor(40, 45); u8g2.print("Score: "); u8g2.print(gscore);
    }
}
void onTap() {
  happiness += 20.0f; if (happiness > 100.0f) happiness = 100.0f;
  static const Expr reactions[] = { E_HAPPY, E_WINK, E_COOL, E_SHY, E_SURPRISED, E_ANGRY, E_FRUSTRATED, E_CURIOUS, E_EXCITED };
  static Expr lastReaction = E_NEUTRAL; Expr nextReaction;
  do { nextReaction = reactions[random(0, 9)]; } while (nextReaction == lastReaction);
  lastReaction = nextReaction; setExpr(nextReaction, 3000); 
  playMelody(sfxPet, 1); setVibe(V_PURR, 300);
}

void onDoubleTap() {
    curMode = MODE_CLOCK; setVibe(V_PULSE_SOFT, 100); clockDisplayState = 0;
}

void handleTouch() {
  static uint32_t pressTime = 0, releaseTime = 0;
  static bool isPressed = false;
  static int tapCount = 0;
  static bool longPressHandled = false;
  static bool extraLongPressHandled = false;
  static bool isLoving = false; 
  static bool stableState = false;
  static uint32_t stateChangeTime = 0;
  
  bool rawState = digitalRead(PIN_TOUCH) == HIGH;
  uint32_t now = millis();

  if (rawState != stableState) {
    if (now - stateChangeTime > 20) { 
      stableState = rawState;
      if (stableState) {
        isPressed = true; pressTime = now; longPressHandled = false; extraLongPressHandled = false;
        gameButtonState = true; gamePressTime = now; 
        if (curMode == MODE_GAME) {
            onGamePress();
        } else {
            wake(); setVibe(V_PULSE_SOFT, 40); 
        }
      } else {
        isPressed = false; releaseTime = now; gameButtonState = false;
        if (curMode != MODE_GAME) {
            if (isLoving) {
              isLoving = false; setExpr(E_NEUTRAL); exprUntil = 0; setVibe(V_OFF);
            } else if (!longPressHandled && !extraLongPressHandled) {
              tapCount++;
            }
        }
      }
    }
  } else {
    stateChangeTime = now;
  }

  if (curMode != MODE_GAME && !isPressed && tapCount > 0 && (now - releaseTime > 400)) {
    if (curMode == MODE_ALARM) {
      curMode = MODE_NORMAL;
    } else if (isFocusMode && tapCount != 3) {
      // Ignore taps
    } else {
      if (tapCount == 1) {
          if (curMode == MODE_CLOCK) {
              if (clockDisplayState == 2) alarmManagerSel = (alarmManagerSel + 1) % 3;
              else clockDisplayState = (clockDisplayState == 0) ? 1 : 0;
          }
          else onTap();
      } else if (tapCount == 2) {
          if (curMode == MODE_CLOCK) {
              if (clockDisplayState == 2) {
                  if (alarmManagerSel == 0) { al1En = !al1En; prefs.putBool("al1En", al1En); }
                  else if (alarmManagerSel == 1) { al2En = !al2En; prefs.putBool("al2En", al2En); }
                  else if (alarmManagerSel == 2) { al3En = !al3En; prefs.putBool("al3En", al3En); }
                  setVibe(V_PULSE_SOFT, 100);
              } else {
                  curMode = MODE_NORMAL;
              }
          }
          else onDoubleTap();
      } else if (tapCount == 3) onTripleTap();
      else if (tapCount >= 4) onQuadTap();
    }
    tapCount = 0;
  }

  if (isPressed) {
      uint32_t holdTime = now - pressTime;
      if (!longPressHandled && holdTime > 1500) {
          longPressHandled = true; // Always consume the event at 1.5s
          if (curMode == MODE_CLOCK) {
              if (clockDisplayState != 2) {
                  clockDisplayState = 2; alarmManagerSel = 0; setVibe(V_PULSE_HARD, 100);
              }
          } else if (curMode == MODE_GAME) {
             curMode = MODE_NORMAL; playMelody(sfxFocusOff, 3); setVibe(V_PULSE_HARD, 200);
          } else if (curMode == MODE_NORMAL && !isFocusMode) {
             isLoving = true; happiness = 100.0f;
             setExpr(E_LOVE, 30000); setVibe(V_HEARTBEAT, 30000); playMelody(sfxLove, 3);
          }
      }
      if (!extraLongPressHandled && holdTime > 3000) {
          extraLongPressHandled = true; // Always consume the event at 3.0s
          if (curMode == MODE_CLOCK && clockDisplayState == 2) {
              clockDisplayState = 0; setVibe(V_PULSE_HARD, 300);
          }
      }
  }
}

void drawWiFiStatus() {
  
  
  if (wifiConnected) {
    // Draw 3 signal bars
    u8g2.drawBox(116, 6, 2, 2);
    u8g2.drawBox(119, 4, 2, 4);
    u8g2.drawBox(122, 2, 2, 6);
  } else {
    // Draw a blinking dot while connecting
    if (millis() % 1000 < 500) {
      u8g2.drawBox(122, 6, 2, 2);
    }
  }
}


// ==============================================================================
// WEB SERVER & ALARM & CLOCK LOGIC
// ==============================================================================

uint32_t lastWeatherFetch = 0;
TaskHandle_t weatherTaskHandle;

void fetchWeatherTask(void * parameter) {
  for(;;) {
    if (wifiConnected && (forceWeatherFetch || millis() - lastWeatherFetch > 900000)) { // 15 mins
      forceWeatherFetch = false;
      HTTPClient http;
      String url = "http://api.openweathermap.org/data/2.5/weather?q=" + weatherCity + "&appid=YOUR_OWM_API_KEY&units=" + weatherUnits;
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          curTemp = doc["main"]["temp"];
          curHumidity = doc["main"]["humidity"];
          curWind = doc["wind"]["speed"];
          String desc = doc["weather"][0]["main"];
          strlcpy(curWeatherDesc, desc.c_str(), sizeof(curWeatherDesc));
          curWeatherId = doc["weather"][0]["id"];
        }
      } else {
        strlcpy(curWeatherDesc, "Net Error", sizeof(curWeatherDesc));
      }
      http.end();
      lastWeatherFetch = millis();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void handleRoot() {

  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
  html += "body{font-family:sans-serif;background:#f4f4f9;color:#333;text-align:center;padding:10px;}";
  html += "h1{color:#ff6b6b;} form{background:#fff;padding:20px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1);display:inline-block;text-align:left;max-width:400px;width:100%;}";
  html += "select, input, button {margin: 5px 0 15px 0; padding: 10px; font-size: 16px; border-radius: 5px; border: 1px solid #ccc; width: 100%; box-sizing: border-box;}";
  html += "button {background:#4ecdc4; color:white; border:none; cursor:pointer; font-weight:bold;} button:hover{background:#45b7d1;}";
  html += "fieldset {border:1px solid #ddd; border-radius:8px; margin-bottom:15px; padding:10px;} legend {font-weight:bold; color:#555;}";
  html += "</style></head><body><h1>Yeti Settings</h1>";
  html += "<form action='/save' method='POST'>";
  
  html += "<fieldset><legend>Timer & Clock</legend>";
  html += "<label>Pomodoro Duration</label>";
  html += "<select name='pomo'>";
  html += "<option value='25_5'" + String((pomoWorkMins==25)?" selected":"") + ">25m Work / 5m Break</option>";
  html += "<option value='50_10'" + String((pomoWorkMins==50)?" selected":"") + ">50m Work / 10m Break</option>";
  html += "<option value='90_30'" + String((pomoWorkMins==90)?" selected":"") + ">90m Work / 30m Break</option>";
  html += "</select>";
  html += "<label>Pomodoro Theme</label>";
  html += "<select name='pomo_th'>";
  html += "<option value='0'" + String((pomoTheme==0)?" selected":"") + ">0: Cute Timer</option>";
  html += "<option value='1'" + String((pomoTheme==1)?" selected":"") + ">1: Determined Yeti</option>";
  html += "<option value='2'" + String((pomoTheme==2)?" selected":"") + ">2: Hourglass</option>";
  html += "</select>";
  html += "<label>Clock Theme</label>";
  html += "<select name='clk_th'>";
  html += "<option value='0'" + String((clockTheme==0)?" selected":"") + ">0: Big Bold</option>";
  html += "<option value='1'" + String((clockTheme==1)?" selected":"") + ">1: Retro Digital</option>";
  html += "<option value='2'" + String((clockTheme==2)?" selected":"") + ">2: Flip Clock</option>";
  html += "</select>";
  html += "</fieldset>";

  html += "<fieldset><legend>Weather Settings</legend>";
  html += "<label>City Name</label>";
  html += "<input type='text' name='w_city' value='" + weatherCity + "' placeholder='e.g. London'>";
  html += "<label>Units</label>";
  html += "<select name='w_units'>";
  html += "<option value='metric'" + String((weatherUnits=="metric")?" selected":"") + ">Celsius</option>";
  html += "<option value='imperial'" + String((weatherUnits=="imperial")?" selected":"") + ">Fahrenheit</option>";
  html += "</select>";
  html += "<label>Weather Theme</label>";
  html += "<select name='w_th'>";
  html += "<option value='0'" + String((weatherTheme==0)?" selected":"") + ">0: Minimal Text</option>";
  html += "<option value='1'" + String((weatherTheme==1)?" selected":"") + ">1: Graphic Icon</option>";
  html += "</select>";
  html += "</fieldset>";

  html += "<fieldset><legend>Alarms</legend>";
  char t1[10], t2[10], t3[10];
  sprintf(t1, "%02d:%02d", al1H, al1M); sprintf(t2, "%02d:%02d", al2H, al2M); sprintf(t3, "%02d:%02d", al3H, al3M);
  
  html += "<label>Alarm 1</label><div style='display:flex;gap:10px;'><input type='time' name='al1_t' value='" + String(t1) + "'>";
  html += "<label style='display:flex;align-items:center;'><input type='checkbox' name='al1_en' " + String(al1En?"checked":"") + "> On</label></div>";
  
  html += "<label>Alarm 2</label><div style='display:flex;gap:10px;'><input type='time' name='al2_t' value='" + String(t2) + "'>";
  html += "<label style='display:flex;align-items:center;'><input type='checkbox' name='al2_en' " + String(al2En?"checked":"") + "> On</label></div>";
  
  html += "<label>Alarm 3</label><div style='display:flex;gap:10px;'><input type='time' name='al3_t' value='" + String(t3) + "'>";
  html += "<label style='display:flex;align-items:center;'><input type='checkbox' name='al3_en' " + String(al3En?"checked":"") + "> On</label></div>";
  
  html += "<label>Alarm Message</label>";
  html += "<input type='text' name='al_msg' maxlength='15' value='" + alMsg + "'>";
  html += "<label>Alarm Sound</label>";
  html += "<select name='al_snd'>";
  html += "<option value='0'" + String((alSound==0)?" selected":"") + ">0: Standard Beep</option>";
  html += "<option value='1'" + String((alSound==1)?" selected":"") + ">1: Gentle Chime</option>";
  html += "<option value='2'" + String((alSound==2)?" selected":"") + ">2: Loud Siren</option>";
  html += "</select>";
  html += "<label style='display:flex;align-items:center;gap:10px;'><input type='checkbox' name='hide_al' style='width:auto;' " + String(hideAlarm?"checked":"") + "> Hide Alarm in Clock Mode</label>";
  html += "</fieldset>";
  
  html += "<button type='submit'>Save Settings</button>";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("pomo")) {
    String p = server.arg("pomo");
    if (p == "25_5") { pomoWorkMins = 25; pomoBreakMins = 5; }
    else if (p == "50_10") { pomoWorkMins = 50; pomoBreakMins = 10; }
    else if (p == "90_30") { pomoWorkMins = 90; pomoBreakMins = 30; }
    prefs.putInt("pomoWork", pomoWorkMins);
    prefs.putInt("pomoBreak", pomoBreakMins);
  }
  if (server.hasArg("pomo_th")) { pomoTheme = server.arg("pomo_th").toInt(); prefs.putInt("pomoThm", pomoTheme); }
  if (server.hasArg("clk_th")) { clockTheme = server.arg("clk_th").toInt(); prefs.putInt("clkThm", clockTheme); }
  
  if (server.hasArg("w_city")) { 
      String newCity = server.arg("w_city");
      String newUnits = server.arg("w_units");
      if (newCity != weatherCity || newUnits != weatherUnits) forceWeatherFetch = true;
      weatherCity = newCity; weatherUnits = newUnits;
      prefs.putString("wCity", weatherCity); prefs.putString("wUnits", weatherUnits);
  }
  if (server.hasArg("w_th")) { weatherTheme = server.arg("w_th").toInt(); prefs.putInt("wTheme", weatherTheme); }

  if (server.hasArg("al1_t")) { String t = server.arg("al1_t"); al1H = t.substring(0, 2).toInt(); al1M = t.substring(3, 5).toInt(); prefs.putInt("al1H", al1H); prefs.putInt("al1M", al1M); }
  if (server.hasArg("al2_t")) { String t = server.arg("al2_t"); al2H = t.substring(0, 2).toInt(); al2M = t.substring(3, 5).toInt(); prefs.putInt("al2H", al2H); prefs.putInt("al2M", al2M); }
  if (server.hasArg("al3_t")) { String t = server.arg("al3_t"); al3H = t.substring(0, 2).toInt(); al3M = t.substring(3, 5).toInt(); prefs.putInt("al3H", al3H); prefs.putInt("al3M", al3M); }
  
  al1En = server.hasArg("al1_en"); prefs.putBool("al1En", al1En);
  al2En = server.hasArg("al2_en"); prefs.putBool("al2En", al2En);
  al3En = server.hasArg("al3_en"); prefs.putBool("al3En", al3En);
  
  if (server.hasArg("al_msg")) { alMsg = server.arg("al_msg"); prefs.putString("alMsg", alMsg); }
  if (server.hasArg("al_snd")) { alSound = server.arg("al_snd").toInt(); prefs.putInt("alSnd", alSound); }
  
  hideAlarm = server.hasArg("hide_al"); prefs.putBool("hideAl", hideAlarm);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void renderFocus(uint32_t now) {
    uint32_t elapsed = now - focusStartTime;
    if (elapsed > focusDur) elapsed = focusDur;
    long secsLeft = (focusDur - elapsed) / 1000;
    
    if (pomoTheme == 0) {
        u8g2.setFont(u8g2_font_logisoso24_tr);
        char buf[8]; sprintf(buf, "%02d:%02d", (int)(secsLeft/60), (int)(secsLeft%60));
        int w = u8g2.getUTF8Width(buf);
        u8g2.drawStr((128-w)/2, 40, buf);
        
        int s = (sin(now / 150.0) + 1.0) * 1.5 + 2;
        drawHeart(64, 55, s);
    } else if (pomoTheme == 2) {
        char buf[8]; sprintf(buf, "%02d:%02d", (int)(secsLeft/60), (int)(secsLeft%60));
        u8g2.setFont(u8g2_font_8x13_tf);
        int w = u8g2.getUTF8Width(buf);
        u8g2.drawStr((128-w)/2, 14, buf);

        int cx = 64, cy = 40, wHalf = 12, hHalf = 16;
        u8g2.drawLine(cx - wHalf, cy - hHalf, cx + wHalf, cy - hHalf);
        u8g2.drawLine(cx - wHalf, cy + hHalf, cx + wHalf, cy + hHalf);
        u8g2.drawLine(cx - wHalf, cy - hHalf, cx, cy);
        u8g2.drawLine(cx + wHalf, cy - hHalf, cx, cy);
        u8g2.drawLine(cx - wHalf, cy + hHalf, cx, cy);
        u8g2.drawLine(cx + wHalf, cy + hHalf, cx, cy);
        
        float progress = (float)elapsed / focusDur;
        
        int drainY = cy - hHalf + (int)(hHalf * progress);
        for (int y = drainY; y < cy; y++) {
            int fillW = (cy - y) * wHalf / hHalf;
            u8g2.drawLine(cx - fillW, y, cx + fillW, y);
        }
        
        int fillStart = cy + hHalf - (int)(hHalf * progress);
        for (int y = fillStart; y <= cy + hHalf; y++) {
            int fillW = (y - cy) * wHalf / hHalf;
            u8g2.drawLine(cx - fillW, y, cx + fillW, y);
        }
        
        if (progress < 1.0f) u8g2.drawLine(cx, cy, cx, fillStart);
    }
}

void renderClock(uint32_t now) {
  struct tm timeinfo;
  if (!wifiConnected || !getLocalTime(&timeinfo, 0)) {
    u8g2.setFont(u8g2_font_8x13_tf);
    int x = (128 - u8g2.getUTF8Width("No Time Sync")) / 2;
    u8g2.drawStr(x, 30, "No Time Sync");
    return;
  }
  
  int h = timeinfo.tm_hour % 12; if (h == 0) h = 12;
  int m = timeinfo.tm_min;

  if (clockDisplayState == 0) {
      if (clockTheme == 0) {
          u8g2.setFont(u8g2_font_logisoso24_tr);
          char tStr[16]; sprintf(tStr, "%02d:%02d", h, m);
          int tw = u8g2.getUTF8Width(tStr);
          int x = (128 - tw) / 2;
          u8g2.drawStr(x - 5, 42, tStr);
          u8g2.setFont(u8g2_font_6x12_tf);
          u8g2.drawStr(x + tw, 42, timeinfo.tm_hour >= 12 ? "PM" : "AM");
      } else if (clockTheme == 1) {
          u8g2.setFont(u8g2_font_freedoomr25_tn);
          char tStr[16]; sprintf(tStr, "%02d:%02d", h, m);
          int tw = u8g2.getUTF8Width(tStr);
          u8g2.drawStr((128 - tw) / 2, 44, tStr);
          u8g2.drawFrame(8, 12, 112, 44);
          u8g2.drawFrame(10, 14, 108, 40);
      } else if (clockTheme == 2) {
          u8g2.setFont(u8g2_font_logisoso24_tr);
          char hStr[8]; sprintf(hStr, "%02d", h);
          char mStr[8]; sprintf(mStr, "%02d", m);
          
          u8g2.drawRBox(14, 12, 42, 40, 4);
          u8g2.drawRBox(72, 12, 42, 40, 4);
          
          u8g2.setDrawColor(0);
          int hw = u8g2.getUTF8Width(hStr);
          u8g2.drawStr(14 + (42-hw)/2, 44, hStr);
          int mw = u8g2.getUTF8Width(mStr);
          u8g2.drawStr(72 + (42-mw)/2, 44, mStr);
          
          u8g2.setDrawColor(1);
          u8g2.drawBox(14, 31, 42, 2);
          u8g2.drawBox(72, 31, 42, 2);
      }
  } else if (clockDisplayState == 1) {
      char tStr[16];
      if (weatherUnits == "metric") sprintf(tStr, "%dC", (int)curTemp);
      else sprintf(tStr, "%dF", (int)curTemp);
      
      u8g2.setFont(u8g2_font_logisoso24_tr);
      int tw = u8g2.getUTF8Width(tStr);
      u8g2.drawStr(120 - tw, 34, tStr);
      
      int iconX = 14, iconY = 22;
      if (curWeatherId >= 200 && curWeatherId < 300) { 
          // Thunderstorm
          u8g2.drawLine(iconX+16, iconY-12, iconX+8, iconY+2);
          u8g2.drawLine(iconX+8, iconY+2, iconX+24, iconY+2);
          u8g2.drawLine(iconX+24, iconY+2, iconX+12, iconY+16);
      } else if (curWeatherId >= 300 && curWeatherId < 600) {
          // Rain
          u8g2.drawCircle(iconX+16, iconY-4, 10);
          u8g2.drawCircle(iconX+8, iconY+2, 6);
          u8g2.drawCircle(iconX+24, iconY+2, 6);
          u8g2.drawLine(iconX+12, iconY+10, iconX+8, iconY+18);
          u8g2.drawLine(iconX+20, iconY+10, iconX+16, iconY+18);
      } else if (curWeatherId >= 600 && curWeatherId < 700) {
          // Snow
          u8g2.drawLine(iconX+16, iconY-12, iconX+16, iconY+12);
          u8g2.drawLine(iconX+4, iconY, iconX+28, iconY);
          u8g2.drawLine(iconX+8, iconY-8, iconX+24, iconY+8);
          u8g2.drawLine(iconX+8, iconY+8, iconX+24, iconY-8);
      } else if (curWeatherId == 800) {
          // Clear Sun
          u8g2.drawCircle(iconX+16, iconY, 8);
          u8g2.drawLine(iconX+16, iconY-12, iconX+16, iconY-16);
          u8g2.drawLine(iconX+16, iconY+12, iconX+16, iconY+16);
          u8g2.drawLine(iconX+4, iconY, iconX, iconY);
          u8g2.drawLine(iconX+28, iconY, iconX+32, iconY);
          u8g2.drawLine(iconX+6, iconY-10, iconX+4, iconY-12);
          u8g2.drawLine(iconX+26, iconY+10, iconX+28, iconY+12);
      } else {
          // Clouds
          u8g2.drawCircle(iconX+12, iconY+4, 6);
          u8g2.drawCircle(iconX+20, iconY-2, 8);
          u8g2.drawCircle(iconX+28, iconY+4, 6);
          u8g2.drawLine(iconX+12, iconY+10, iconX+28, iconY+10);
      }
      
      u8g2.setFont(u8g2_font_5x7_tf);
      char w1[64]; sprintf(w1, "%s", curWeatherDesc);
      u8g2.drawStr((128 - u8g2.getUTF8Width(w1)) / 2, 46, w1);
      
      char w2[64]; sprintf(w2, "Hum: %d%% | Wind: %.1f", curHumidity, curWind);
      u8g2.drawStr((128 - u8g2.getUTF8Width(w2)) / 2, 58, w2);
      
  } else if (clockDisplayState == 2) {
      u8g2.setFont(u8g2_font_8x13_tf);
      u8g2.drawStr(38, 16, "ALARMS");
      
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(8, 62, "[1Tap] Next [2Tap] Toggle");

      int ah = (alarmManagerSel==0) ? al1H : (alarmManagerSel==1 ? al2H : al3H);
      int am = (alarmManagerSel==0) ? al1M : (alarmManagerSel==1 ? al2M : al3M);
      bool aEn = (alarmManagerSel==0) ? al1En : (alarmManagerSel==1 ? al2En : al3En);
      
      int dAh = ah % 12; if (dAh == 0) dAh = 12;
      
      char tStr[32]; sprintf(tStr, "ALARM %d: %02d:%02d %s", alarmManagerSel+1, dAh, am, ah >= 12 ? "PM" : "AM");
      int tw = u8g2.getUTF8Width(tStr);
      u8g2.drawStr((128-tw)/2, 34, tStr);
      
      u8g2.setFont(u8g2_font_8x13_tf);
      char sStr[16]; sprintf(sStr, "[%s]", aEn ? "ON " : "OFF");
      tw = u8g2.getUTF8Width(sStr);
      u8g2.drawStr((128-tw)/2, 50, sStr);
  }
}

void renderAlarm(uint32_t now) {
  if ((now / 500) % 2 == 0) {
    u8g2.setFont(u8g2_font_8x13_tf);
    int x = (128 - u8g2.getUTF8Width(alMsg.c_str())) / 2;
    u8g2.drawStr(x, 26, alMsg.c_str());
    x = (128 - u8g2.getUTF8Width("(Tap to Stop)")) / 2;
    u8g2.drawStr(x, 50, "(Tap to Stop)");
  } else {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0,0,128,64);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_8x13_tf);
    int x = (128 - u8g2.getUTF8Width(alMsg.c_str())) / 2;
    u8g2.drawStr(x, 36, alMsg.c_str());
    u8g2.setDrawColor(1);
  }
  if ((now / 200) % 2 == 0) {
    if (alSound == 0) playMelody(sfxWake, 1);
    else if (alSound == 1) playMelody(sfxChime, 1);
    else if (alSound == 2) playMelody(sfxSiren, 1);
    setVibe(V_PULSE_HARD, 100);
  }
}
void setup() {
  Serial.begin(115200); pinMode(PIN_TOUCH, INPUT_PULLDOWN); pinMode(PIN_BUZZER, OUTPUT);
  analogWriteFrequency(PIN_VIBE, 20000); analogWriteResolution(PIN_VIBE, 8); pinMode(PIN_VIBE, OUTPUT); analogWrite(PIN_VIBE, 0);
  randomSeed(esp_random()); prefs.begin("deskcomp", false);
  
  pomoWorkMins = prefs.getInt("pomoWork", 25);
  pomoBreakMins = prefs.getInt("pomoBreak", 5);
  hideAlarm = prefs.getBool("hideAl", false);
  al1H = prefs.getInt("al1H", 7); al1M = prefs.getInt("al1M", 0); al1En = prefs.getBool("al1En", false);
  al2H = prefs.getInt("al2H", 8); al2M = prefs.getInt("al2M", 0); al2En = prefs.getBool("al2En", false);
  al3H = prefs.getInt("al3H", 9); al3M = prefs.getInt("al3M", 0); al3En = prefs.getBool("al3En", false);
  weatherCity = prefs.getString("wCity", "London");
  weatherUnits = prefs.getString("wUnits", "metric");
  weatherTheme = prefs.getInt("wTheme", 1);
  pomoTheme = prefs.getInt("pomoThm", 0);
  clockTheme = prefs.getInt("clkThm", 0);
  alMsg = prefs.getString("alMsg", "WAKE UP!");
  alSound = prefs.getInt("alSnd", 0);

  Wire.setPins(PIN_SDA, PIN_SCL); u8g2.begin(); u8g2.setBusClock(400000); u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.clearBuffer(); int x = (128 - u8g2.getUTF8Width("YETI V2.0")) / 2; u8g2.drawStr(x, 36, "YETI V2.0"); u8g2.sendBuffer();
  playMelody(sfxWake, 2); setVibe(V_PULSE_HARD, 150); delay(1000);

  u8g2.clearBuffer(); u8g2.drawStr(5, 30, "Connecting Wi-Fi..."); u8g2.sendBuffer();
  WiFi.disconnect(true); WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  uint32_t startWait = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWait < 15000) { delay(100); }
  
  u8g2.clearBuffer();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    localIP = WiFi.localIP().toString();
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    u8g2.drawStr(5, 20, "Connected!"); u8g2.drawStr(5, 40, "IP:"); u8g2.drawStr(5, 55, localIP.c_str());
  } else {
    wifiFailed = true; WiFi.mode(WIFI_OFF);
    u8g2.drawStr(5, 30, "Offline Mode");
  }
  u8g2.sendBuffer(); delay(5000); // Hold for user to read IP

  cur = PRESETS[E_SLEEP]; tgt = PRESETS[E_NEUTRAL];
  uint32_t now = millis(); lastTouchAt = now; lastIdleEventAt = now; lastFrame = now; prevUpdateTime = now;
  setExpr(E_NEUTRAL);

  xTaskCreate(
    fetchWeatherTask,
    "WeatherTask",
    8192,
    NULL,
    1,
    &weatherTaskHandle
  );
}

void loop() {
  uint32_t now = millis();
  handleTouch(); updateVibe(now); updateMelody(now);
  
  if (wifiConnected) { server.handleClient(); }
  
  // Check Alarm
  if (wifiConnected && curMode != MODE_ALARM) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
      bool trigger = false;
      if (al1En && timeinfo.tm_hour == al1H && timeinfo.tm_min == al1M) trigger = true;
      if (al2En && timeinfo.tm_hour == al2H && timeinfo.tm_min == al2M) trigger = true;
      if (al3En && timeinfo.tm_hour == al3H && timeinfo.tm_min == al3M) trigger = true;
      
      if (trigger && timeinfo.tm_min != lastAlarmCheckMinute) {
          curMode = MODE_ALARM;
          setExpr(E_SURPRISED);
      }
      lastAlarmCheckMinute = timeinfo.tm_min;
    }
  }
  
  if (now - lastFrame >= FRAME_MS) {
    lastFrame += FRAME_MS; if (now - lastFrame > FRAME_MS * 3) lastFrame = now;
    
    if (curMode == MODE_GAME) updateGame(now);
    else if (curMode == MODE_CLOCK || curMode == MODE_ALARM) {} // No updates needed
    else updateNormal(now);

    if (!sleeping || (now - lastTouchAt <= 600000) || curMode != MODE_NORMAL) {
      u8g2.clearBuffer(); u8g2.setDrawColor(1); 
      
      if (curMode == MODE_GAME) renderGame(now);
      else if (curMode == MODE_CLOCK) renderClock(now);
      else if (curMode == MODE_ALARM) renderAlarm(now);
      else if (isFocusMode && pomoTheme != 1) renderFocus(now);
      else renderNormal(now);
      
      u8g2.sendBuffer();
    } else { delay(10); }
  }
}
