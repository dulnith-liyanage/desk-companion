# Yeti V2.0 - Desktop Companion

A cute, highly expressive, standalone desktop companion built for the ESP32-C3 Super Mini. 

Yeti V2.0 features a completely procedural 30FPS animation engine (smoothly morphing between expressions without using static GIFs), a rich haptic feedback system, built-in mini-games, and a distraction-free Pomodoro focus mode.

## Features

*   **Procedural Animation Engine (30FPS):** Math-based rendering allows for incredibly smooth tweening between 19 unique, adorable expressions. Features dynamic blinking, head tilting, and room-gazing logic.
*   **Tamagotchi Personality:** Yeti has a built-in happiness system. Leave it alone and it will occasionally hum a tune, get curious, doze off, sneeze, or become lonely.
*   **Premium Haptics & Audio:** Uses PWM signals to create nuanced vibrations (purring, breathing, heartbeats, heavy thumps) synchronized with retro 8-bit audio chimes.
*   **Focus Mode:** A built-in 25-minute Pomodoro timer that halts distractions. The Yeti puts on a determined face to study alongside you.
*   **Mini-Games & Secrets:** Includes a built-in "Yeti Run" game and a quirky hidden message.

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

*   **1 Tap (Pet):** Pets the Yeti! (Increases happiness, purrs, and shows a cute reaction).
*   **2 Taps (Focus):** Toggles the 25-minute Pomodoro Focus Mode.
*   **3 Taps (Secret):** Triggers a quirky, animated "I LOVE YOU!" sequence.
*   **4 Taps (Yeti Run):** Enters the "Yeti Run" mini-game. Tap to jump over obstacles. Long hold to exit.
*   **Long Hold (Normal Mode):** Triggers the exclusive "Love" mode with giant heart eyes and heartbeat vibrations.

## Setup Instructions

1.  Open `desk_companion.ino` in the Arduino IDE.
2.  Install the **U8g2** library via the Library Manager.
3.  Select the **ESP32C3 Dev Module** board.
4.  Upload the sketch to your board.
