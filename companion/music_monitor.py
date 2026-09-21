#!/usr/bin/env python3
"""
Desk Companion - Music / Audio Monitor
======================================
Monitors whether music/audio is playing on your laptop and sends the playback
status to the ESP32-C3 Desk Companion over USB serial:
    'M' - Music / audio started playing
    'm' - Music / audio stopped playing

Cross-Platform Audio Detection:
--------------------------------
- macOS:
    1. Checks Spotify and Apple Music via AppleScript (osascript) for instant
       response. Checks whether applications are running first to prevent
       accidentally launching them.
    2. Falls back to `pmset -g assertions` to detect system-wide audio output
       (browser YouTube, VLC, media players, etc.).
- Windows:
    Uses `pycaw` IAudioMeterInformation peak meter (GetPeakValue > 0.005) on the
    default audio output device. Caches the meter object and re-initializes on
    device change.
- Linux:
    Checks PulseAudio / PipeWire sink inputs using `pactl list sink-inputs`
    for 'Corked: no'. Falls back to ALSA procfs at
    `/proc/asound/card*/pcm*p/sub*/status` looking for 'state: RUNNING'.

ESP32 Serial Communication:
---------------------------
- Auto-detects ESP32-C3 port by USB Vendor ID 0x303A (Espressif).
- Falls back to /dev/cu.usbmodem* on macOS, /dev/ttyACM* on Linux.
- Opens serial at 115200 baud WITHOUT triggering ESP32 reset: sets `dtr=False`
  and `rts=False` before calling `open()`.
- Sends 'M' on audio start, 'm' on audio stop.
- Sends a periodic heartbeat ('M' or 'm') every 30 seconds to keep sync.
- Auto-reconnects if connection is lost.
- Accepts optional `--port` CLI argument to override auto-detection.

Installation & Running:
-----------------------
Requirements note:
- `pyserial` is required on all platforms.
- `pycaw` and `comtypes` are needed on Windows only.

To install dependencies:
    # macOS / Linux:
    pip install pyserial

    # Windows:
    pip install pyserial pycaw comtypes

To run:
    python music_monitor.py
    # or with a specific serial port:
    python music_monitor.py --port /dev/cu.usbmodem2101
"""

import argparse
import glob
import subprocess
import sys
import time

# Guard pyserial import with a friendly error message
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print(
        "Error: 'pyserial' is required but not installed.\n"
        "Please install it with: pip install pyserial",
        file=sys.stderr,
    )
    sys.exit(1)

# Platform-specific imports for Windows audio monitoring
if sys.platform.startswith("win"):
    try:
        from pycaw.pycaw import AudioUtilities, IAudioMeterInformation
        import comtypes
    except ImportError:
        try:
            from pycaw.utils import AudioUtilities
            from pycaw.api.endpointvolume import IAudioMeterInformation
            import comtypes
        except ImportError:
            AudioUtilities = None
            IAudioMeterInformation = None
            comtypes = None

# Polling and heartbeat timing configuration (seconds)
POLL_INTERVAL = 2.0
HEARTBEAT_INTERVAL = 30.0

# Windows peak meter cache
_windows_meter = None


# ==============================================================================
# Audio Detection Functions
# ==============================================================================

def is_audio_playing_macos() -> bool:
    """Detect whether audio is playing on macOS.

    First checks Spotify and Apple Music via AppleScript (instant response),
    checking if the application is running first to avoid launching it.
    If neither is playing, falls back to `pmset -g assertions` for system-wide
    audio output (browser YouTube, VLC, etc.).
    """
    # 1. Spotify / Apple Music via osascript
    script = """
    set appState to "none"
    if application "Spotify" is running then
        tell application "Spotify"
            if player state is playing then return "playing"
            if player state is paused then set appState to "paused"
        end tell
    end if
    if application "Music" is running then
        tell application "Music"
            if player state is playing then return "playing"
            if player state is paused then set appState to "paused"
        end tell
    end if
    return appState
    """
    try:
        res = subprocess.run(
            ["osascript", "-e", script],
            capture_output=True,
            text=True,
            timeout=1,
        )
        out = res.stdout.strip()
        if res.returncode == 0:
            if out == "playing":
                return True
            if out == "paused":
                return False
    except Exception:
        pass

    # 2. Fall back to pmset -g assertions for browser/VLC/system-wide audio
    try:
        res = subprocess.run(
            ["pmset", "-g", "assertions"],
            capture_output=True,
            text=True,
            timeout=1,
        )
        if res.returncode == 0:
            output = res.stdout
            for line in output.splitlines():
                line_lower = line.lower()
                # Active audio playback triggers coreaudiod / com.apple.audio sleep prevention
                if "coreaudiod" in line_lower or "com.apple.audio" in line_lower:
                    # Ignore background Siri / dictation listener aggregate device
                    if "avvcaggregatedevice" in line_lower:
                        continue
                    if "preventuseridlesleep" in line_lower or "preventuseridlesystemsleep" in line_lower:
                        return True
    except Exception:
        pass

    return False


def _get_windows_audio_meter():
    """Retrieve and cache the Windows IAudioMeterInformation peak meter."""
    global _windows_meter
    if _windows_meter is not None:
        return _windows_meter

    if AudioUtilities is None or IAudioMeterInformation is None or comtypes is None:
        return None

    try:
        speakers = AudioUtilities.GetSpeakers()
        if not speakers:
            return None
        interface = speakers.Activate(IAudioMeterInformation._iid_, comtypes.CLSCTX_ALL, None)
        _windows_meter = interface.QueryInterface(IAudioMeterInformation)
        return _windows_meter
    except Exception:
        _windows_meter = None
        return None


def is_audio_playing_windows() -> bool:
    """Detect whether audio is playing on Windows using pycaw peak meter.

    Uses GetPeakValue > 0.005. Caches the meter object and re-initializes
    when the audio endpoint changes or on COM error.
    """
    global _windows_meter

    if AudioUtilities is None:
        return False

    meter = _get_windows_audio_meter()
    if meter is None:
        return False

    try:
        peak = meter.GetPeakValue()
        return peak > 0.005
    except Exception:
        # Re-initialize on device change or error
        _windows_meter = None
        meter = _get_windows_audio_meter()
        if meter is not None:
            try:
                return meter.GetPeakValue() > 0.005
            except Exception:
                _windows_meter = None
        return False


def is_audio_playing_linux() -> bool:
    """Detect whether audio is playing on Linux.

    Checks `pactl list sink-inputs` for 'Corked: no'.
    Falls back to `/proc/asound/card*/pcm*p/sub*/status` for 'state: RUNNING'.
    """
    # 1. PulseAudio / PipeWire via pactl
    try:
        res = subprocess.run(
            ["pactl", "list", "sink-inputs"],
            capture_output=True,
            text=True,
            timeout=1,
        )
        if res.returncode == 0:
            for line in res.stdout.splitlines():
                if "corked: no" in line.lower():
                    return True
    except Exception:
        pass

    # 2. Fall back to ALSA procfs
    try:
        status_files = glob.glob("/proc/asound/card*/pcm*p/sub*/status")
        for sf in status_files:
            try:
                with open(sf, "r") as f:
                    if "state: RUNNING" in f.read():
                        return True
            except OSError:
                pass
    except Exception:
        pass

    return False


def is_audio_playing() -> bool:
    """Detect whether audio is currently playing on the host platform."""
    if sys.platform == "darwin":
        return is_audio_playing_macos()
    elif sys.platform.startswith("win"):
        return is_audio_playing_windows()
    elif sys.platform.startswith("linux"):
        return is_audio_playing_linux()
    return False


# ==============================================================================
# Serial Communication Functions
# ==============================================================================

def find_serial_port(specified_port: str = None) -> str:
    """Find the serial port for the ESP32-C3.

    If specified_port is provided, returns it.
    Otherwise searches for USB Vendor ID 0x303A (Espressif),
    falling back to OS-specific serial port paths.
    """
    if specified_port:
        return specified_port

    # 1. Search comports for USB Vendor ID 0x303A (Espressif)
    try:
        for p in serial.tools.list_ports.comports():
            if p.vid == 0x303A:
                return p.device
            if p.hwid and "303A:" in p.hwid.upper():
                return p.device
    except Exception:
        pass

    # 2. Platform fallback patterns
    if sys.platform == "darwin":
        modems = sorted(glob.glob("/dev/cu.usbmodem*"))
        if modems:
            return modems[0]
    elif sys.platform.startswith("linux"):
        acm_ports = sorted(glob.glob("/dev/ttyACM*"))
        if acm_ports:
            return acm_ports[0]

    return None


def open_serial(port: str) -> serial.Serial:
    """Open serial port at 115200 baud without triggering ESP32 reset.

    DTR and RTS are set to False before calling open() to prevent resetting
    the microcontroller on connection.
    """
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 1
    ser.open()
    # ESP32-C3 native USB CDC needs time to stabilize after host opens port.
    # Without this delay, the first bytes sent are lost during USB re-enumeration.
    time.sleep(2.0)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


def send_command(ser: serial.Serial, cmd: str):
    """Send a single character command to the ESP32."""
    if ser is None or not ser.is_open:
        raise serial.SerialException("Serial port is not open")
    ser.write(cmd.encode("ascii"))
    ser.flush()


# ==============================================================================
# Main Loop & CLI
# ==============================================================================

def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Monitor computer audio playback and notify ESP32-C3 desk companion over serial."
    )
    parser.add_argument(
        "--port", "-p",
        type=str,
        default=None,
        help="Serial port of ESP32-C3 (e.g. /dev/cu.usbmodem2101, COM3). Auto-detected if omitted.",
    )
    return parser.parse_args()


def main():
    """Main execution loop."""
    args = parse_args()

    print("=" * 60)
    print("Desk Companion - Music & Audio Monitor")
    print(f"Platform: {sys.platform}")
    if args.port:
        print(f"Target port: {args.port} (user specified)")
    else:
        print("Target port: Auto-detecting ESP32-C3 (VID 0x303A)...")
    print(f"Polling interval: {POLL_INTERVAL}s | Heartbeat: {HEARTBEAT_INTERVAL}s")
    print("Press Ctrl+C to stop.")
    print("=" * 60)

    ser = None
    current_port = None
    last_audio_state = None  # None: uninitialized; True: playing; False: stopped
    last_heartbeat_time = 0.0
    waiting_message_printed = False

    try:
        while True:
            current_time = time.time()

            # 1. Connect or reconnect serial if not open
            if ser is None or not ser.is_open:
                port = find_serial_port(args.port)
                if port:
                    try:
                        ser = open_serial(port)
                        current_port = port
                        waiting_message_printed = False
                        print(f"[Serial] Connected to ESP32 on {port} (115200 baud, reset prevented)")

                        # Force-sync current audio state immediately after connecting
                        sync_state = is_audio_playing()
                        cmd = "M" if sync_state else "m"
                        send_command(ser, cmd)
                        last_audio_state = sync_state
                        last_heartbeat_time = current_time
                        status_str = "Playing" if sync_state else "Stopped"
                        print(f"[Serial] Synced current status: '{cmd}' ({status_str})")
                    except (serial.SerialException, OSError) as e:
                        ser = None
                        if not waiting_message_printed:
                            print(f"[Serial] Failed to connect to {port}: {e}. Retrying...")
                            waiting_message_printed = True
                else:
                    if not waiting_message_printed:
                        if args.port:
                            print(f"[Serial] Specified port '{args.port}' not found. Waiting...")
                        else:
                            print("[Serial] ESP32 not detected. Waiting for device...")
                        waiting_message_printed = True

            # 2. Check current audio status
            audio_playing = is_audio_playing()

            # 3. Send command on state change
            if audio_playing != last_audio_state:
                cmd = "M" if audio_playing else "m"
                status_str = "Playing" if audio_playing else "Stopped"
                print(f"[Audio] State changed: {status_str} -> sending '{cmd}'")
                last_audio_state = audio_playing
                last_heartbeat_time = current_time

                if ser is not None and ser.is_open:
                    try:
                        send_command(ser, cmd)
                    except (serial.SerialException, OSError) as e:
                        print(f"[Serial] Connection lost while sending state change: {e}")
                        try:
                            ser.close()
                        except Exception:
                            pass
                        ser = None

            # 4. Periodic heartbeat (every 30 seconds)
            elif current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL:
                cmd = "M" if audio_playing else "m"
                status_str = "Playing" if audio_playing else "Stopped"
                last_heartbeat_time = current_time

                if ser is not None and ser.is_open:
                    try:
                        send_command(ser, cmd)
                        print(f"[Heartbeat] Sent periodic '{cmd}' ({status_str})")
                    except (serial.SerialException, OSError) as e:
                        print(f"[Serial] Connection lost while sending heartbeat: {e}")
                        try:
                            ser.close()
                        except Exception:
                            pass
                        ser = None

            # 5. Wait until next poll
            time.sleep(POLL_INTERVAL)

    except KeyboardInterrupt:
        print("\n[Exit] Keyboard interrupt received. Exiting gracefully...")
    finally:
        if ser is not None and ser.is_open:
            try:
                ser.close()
                print(f"[Serial] Connection on {current_port} closed.")
            except Exception:
                pass
        print("[Exit] Music monitor stopped.")


if __name__ == "__main__":
    main()
