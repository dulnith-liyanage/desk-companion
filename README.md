# Desk Companion

A cute, expressive desktop companion built for the ESP32-C3 Super Mini with an I2C OLED display.

The robot features 13 different expressions, smooth time-based animation (no jitter!), capacitive touch support for interactions, and a background macOS companion script that automatically detects when you're playing music on your computer and updates the robot's face to wear headphones.

## Hardware Requirements

*   **ESP32-C3 Super Mini** (or similar ESP32 board)
*   **0.96" or 1.3" I2C OLED Display** (SSD1306 or SH1106)
*   **TTP223 Capacitive Touch Sensor** (or similar)
*   Jumper wires

## Pin Wiring

| Component | ESP32-C3 Pin |
| :--- | :--- |
| **OLED SDA** | GPIO 5 |
| **OLED SCL** | GPIO 6 |
| **Touch Sensor** | GPIO 9 |

## Setup Instructions

### 1. Flash the ESP32

1.  Open `desk_companion.ino` in the Arduino IDE.
2.  Install the **U8g2** library via the Library Manager.
3.  Select the **ESP32C3 Dev Module** board.
4.  **Important**: Set **USB CDC On Boot** to **Enabled** in the Tools menu. This is required for the music detection to work!
5.  Upload the sketch to your board.

### 2. Setup the Music Monitor (macOS)

The companion script runs silently in the background on your Mac and detects when Spotify, Apple Music, or browser audio is playing. It then sends a command over USB to the ESP32 to show the headphones animation.

1.  Open your Terminal.
2.  Navigate to the companion directory:
    ```bash
    cd companion
    ```
3.  Run the one-time setup script:
    ```bash
    ./setup.sh
    ```
    This will create a Python virtual environment, install the required `pyserial` library, and register a macOS Launch Agent so the script starts automatically every time you log in.

*(If you ever want to remove the background agent, just run `./uninstall.sh`)*

## Touch Controls

*   **Single Tap**: Cycle through all 13 expressions.
*   **Double Tap**: Toggle "Emotional Mode" on/off. When enabled, the robot will automatically cycle through different moods on its own.
*   **Long Press (3s)**: Force the "Love" expression (hearts).

## Troubleshooting

*   **Face not changing when music plays**: Ensure you flashed the ESP32 with `USB CDC On Boot: Enabled`. Check the monitor logs at `~/Library/Logs/DeskCompanion/music_monitor.log`.
*   **Glitching graphics / lines**: Ensure you are using the latest version of the U8g2 library.
