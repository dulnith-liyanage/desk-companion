# Yeti V2.0 - Desktop Companion

A cute, highly expressive, standalone desktop companion built for the ESP32-C3 Super Mini. 

Yeti V2.0 features a completely procedural 30FPS animation engine (smoothly morphing between expressions without using static GIFs), a rich haptic feedback system, built-in mini-games, and a distraction-free Pomodoro focus mode.

## New Features in V2.0
*   **Web Dashboard & Wi-Fi:** When Yeti boots up, it connects to your local Wi-Fi and hosts a mobile-friendly dashboard you can access via its IP address.
*   **Pomodoro Focus Mode:** Start a study session alongside Yeti! You can select between 25/5, 50/10, or 90/30 work/break splits via the web dashboard.
*   **NTP Alarm Clock:** Yeti syncs with global time servers. Set your alarm via the web dashboard and Yeti will buzz and vibrate to wake you up!
*   **Procedural Animation Engine (30FPS):** Math-based rendering allows for incredibly smooth tweening between 19 unique, adorable expressions. Features dynamic blinking, head tilting, and room-gazing logic.
*   **Tamagotchi Personality:** Yeti has a built-in happiness system. Leave it alone and it will occasionally hum a tune, get curious, doze off, sneeze, or become lonely.

## Hardware Requirements

*   **ESP32-C3 Super Mini** (or similar ESP32 board)
*   **0.96" or 1.3" I2C OLED Display** (SSD1306)
*   **TTP223 Capacitive Touch Sensor** (or any digital touch module)
*   **Piezo Buzzer** (Passive)
*   **Vibration Motor** (Coin style)
*   Jumper wires & Breadboard

## Pin Wiring

| Component | ESP32-C3 Pin |
| :--- | :--- |
| **OLED SDA** | GPIO 8 |
| **OLED SCL** | GPIO 9 |
| **Touch Sensor** | GPIO 3 |
| **Buzzer** | GPIO 5 |
| **Vibration Motor** | GPIO 10 |

## Touch Controls

*   **1 Tap (Pet):** Pets the Yeti! (Increases happiness, purrs, and shows a cute reaction). Dismisses Alarms.
*   **2 Taps (Focus):** Toggles the Pomodoro Focus Mode.
*   **3 Taps (Clock):** Opens the Clock Screen to show the current time and active Alarm.
*   **4 Taps (Yeti Run):** Enters the "Yeti Run" mini-game. Tap to jump over obstacles. Long hold to exit.
*   **Long Hold (Normal Mode):** Triggers the exclusive "Love" mode with giant heart eyes and heartbeat vibrations.

## Setup Instructions

1.  Open `desk_companion.ino` in the Arduino IDE.
2.  Install the **U8g2** library via the Library Manager.
3.  Update lines 13 and 14 with your Wi-Fi SSID and Password.
4.  Select the **ESP32C3 Dev Module** board.
5.  Upload the sketch to your board.
6.  Look at the Yeti screen during boot to find its IP address. Type this into your phone/laptop browser to access the Yeti Settings Dashboard!
