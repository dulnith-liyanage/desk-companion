# 🧊 Desk Companion — Yeti Edition

A feature-rich ESP32-C3 desk companion with an OLED face, capacitive touch controls, vibration motor, buzzer, Wi-Fi, and a web dashboard.

---

## ✨ Features

### 🐾 Living Face
The Yeti has an animated OLED face with procedurally generated expressions that react to touch, time, and environment:
- Neutral, Happy, Sad, Surprised, Skeptical, Love, Determined, Frustrated
- Idle animations: blinking, yawning, sneezing, looking around
- Happiness system that decays over time — pet your Yeti to keep it happy!

### 👆 Touch Controls
| Gesture | Action |
|---|---|
| **Single Tap** | Random happy reaction |
| **Double Tap** | Enter / Exit Clock Mode |
| **Triple Tap** | Toggle Pomodoro Focus Mode |
| **Quad Tap (4x)** | Start Yeti Run mini-game |
| **Long Press (1.5s)** | Pet mode (Love expression) / Enter Alarm Manager in Clock Mode |
| **Extra-Long Press (3s)** | Exit Alarm Manager back to Clock |

### ⏰ Clock Mode
Entered with a double tap. Has three sub-screens cycled with a single tap:
1. **Clock** — Large time display (theme-selectable)
2. **Weather Dashboard** — Full-screen weather with graphic icons, temperature, humidity, and wind
3. **Alarm Manager** — Cycle through and toggle alarms directly from the Yeti (long press to enter, extra-long press to exit)

**Clock Themes** (configurable from dashboard):
- `0` — Big Bold: Large digital numbers
- `1` — Retro Digital: Blocky font inside a double-frame border
- `2` — Flip Clock: Split-panel flip clock style

### 🌤️ Live Weather
Fetches from [OpenWeatherMap](https://openweathermap.org/) every 15 minutes via a background FreeRTOS task. Supports Celsius/Fahrenheit. Graphic icons: ☀️ Sunny, ☁️ Cloudy, 🌧️ Rain, ❄️ Snow, ⛈️ Thunderstorm.

### 🔔 Multiple Alarms
Supports up to **3 independent alarms**. The Yeti automatically detects and displays the next upcoming alarm. Alarms can be toggled on/off from the web dashboard or directly from the Yeti via the Alarm Manager screen.

### 🍅 Pomodoro Focus Timer
Triple-tap to enter/exit. Configurable work/break durations.

**Pomodoro Themes** (configurable from dashboard):
- `0` — Cute Timer: Large countdown with a pulsing heart
- `1` — Determined Yeti: Yeti face with time shown at the top
- `2` — Hourglass: Animated pixel-art hourglass that drains in real time

### 🎮 Yeti Run (Mini-Game)
A Dino-style endless runner. Tap to jump over cacti and low birds. High birds fly safely overhead — don't jump! Your score increases every frame you survive.

### 😴 Sleep Mode
The Yeti dims and sleeps after 10 minutes of inactivity. Any touch wakes it up.

---

## 🌐 Web Dashboard

Connect to the Yeti's IP address in your browser (shown on OLED at startup) to configure:

- **Pomodoro Duration:** 25/5, 50/10, 90/30
- **Pomodoro Theme:** Cute Timer, Determined Yeti, Hourglass
- **Clock Theme:** Big Bold, Retro Digital, Flip Clock
- **Weather City & Units:** Any city, Celsius or Fahrenheit
- **Weather Display Theme:** Minimal Text or Graphic Icon
- **Up to 3 Alarms:** Set time, enable/disable, custom message, custom sound
- **Alarm Sound:** Standard Beep, Gentle Chime, Loud Siren
- **Hide Alarm in Clock Mode:** For a clean clock-only view

---

## 🔧 Hardware

| Component | Pin |
|---|---|
| SSD1306 OLED (128×64, I2C) | SDA: GPIO6, SCL: GPIO7 |
| Capacitive Touch Sensor | GPIO2 |
| Vibration Motor | GPIO10 |
| Passive Buzzer | GPIO3 |

**Board:** ESP32-C3 (with USB CDC enabled)

---

## 🚀 Setup

1. Install **arduino-cli** or Arduino IDE
2. Install board: `esp32:esp32` v3.x
3. Install libraries: `U8g2`, `ArduinoJson`
4. Set FQBN: `esp32:esp32:esp32c3:CDCOnBoot=cdc`
5. Flash and connect to your Wi-Fi via the auto-connect (credentials hardcoded in `desk_companion.ino`)
6. Open the IP shown on the OLED in your browser to configure

---

## 📁 Project Structure

```
desk_companion/
├── desk_companion.ino   # Main firmware (single-file Arduino sketch)
├── companion/
│   └── music_monitor.py # Optional: Python script to detect music playback
└── README.md
```
