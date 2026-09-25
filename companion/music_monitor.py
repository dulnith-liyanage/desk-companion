#!/usr/bin/env python3
"""
Desk Companion - Laptop Context Monitor
Monitors active apps, battery, CPU, and music, sending state to the ESP32.
"""
import os
import sys
import time
import subprocess
import argparse
import serial
import serial.tools.list_ports

POLL_INTERVAL = 2.0

def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)

def find_esp32_port() -> str:
    for port in serial.tools.list_ports.comports():
        if port.vid == 0x303A:
            return port.device
    
    import glob
    for pattern in ["/dev/cu.usbmodem*", "/dev/ttyACM*"]:
        matches = glob.glob(pattern)
        if matches:
            return matches[0]
    return None

def is_screen_locked() -> bool:
    if sys.platform != "darwin": return False
    try:
        res = subprocess.run(["ioreg", "-n", "Root", "-d1"], capture_output=True, text=True, timeout=1)
        return "CGSSessionScreenIsLocked" in res.stdout
    except:
        return False

def get_active_app() -> str:
    if sys.platform != "darwin": return ""
    try:
        res = subprocess.run(["lsappinfo", "front"], capture_output=True, text=True, timeout=1)
        asn = res.stdout.strip()
        if asn:
            res2 = subprocess.run(["lsappinfo", "info", "-only", "bundleid", asn], capture_output=True, text=True, timeout=1)
            # Output format: "CFBundleIdentifier"="com.apple.Safari"
            out = res2.stdout.lower()
            return out
    except:
        pass
    return ""

def get_battery_state() -> tuple:
    if sys.platform != "darwin": return False, False
    try:
        res = subprocess.run(["pmset", "-g", "batt"], capture_output=True, text=True, timeout=1)
        out = res.stdout.lower()
        is_charging = "ac power" in out
        is_low = False
        if "discharging" in out:
            import re
            match = re.search(r'(\d+)%', out)
            if match and int(match.group(1)) <= 20:
                is_low = True
        return is_low, is_charging
    except:
        pass
    return False, False

def get_cpu_stressed() -> bool:
    if sys.platform != "darwin": return False
    try:
        res = subprocess.run(["ps", "-A", "-o", "%cpu"], capture_output=True, text=True, timeout=1)
        lines = res.stdout.splitlines()[1:]
        total = sum(float(l.strip()) for l in lines if l.strip())
        cores = os.cpu_count() or 4
        return (total / cores) > 70.0
    except:
        pass
    return False

def is_audio_playing() -> bool:
    if sys.platform != "darwin": return False
    try:
        res = subprocess.run(["pmset", "-g", "assertions"], capture_output=True, text=True, timeout=1)
        for line in res.stdout.splitlines():
            line_lower = line.lower()
            if "coreaudiod" in line_lower or "com.apple.audio" in line_lower:
                if "avvcaggregatedevice" in line_lower:
                    continue
                import re
                if re.search(r'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}', line_lower):
                    continue
                if "preventuseridlesleep" in line_lower or "preventuseridlesystemsleep" in line_lower:
                    return True
    except:
        pass
    return False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="Serial port of the ESP32")
    args = parser.parse_args()

    print("=" * 60)
    print("Desk Companion - Context Monitor")
    print(f"Polling interval: {POLL_INTERVAL}s")
    print("Press Ctrl+C to stop.")
    print("=" * 60, flush=True)

    while True:
        target_port = args.port or find_esp32_port()
        if not target_port:
            log("No ESP32 detected. Waiting...")
            time.sleep(5)
            continue

        try:
            ser = serial.Serial()
            ser.port = target_port
            ser.baudrate = 115200
            ser.timeout = 1
            ser.dtr = True
            ser.rts = False
            ser.open()
            log(f"Connected to {target_port}")
            time.sleep(2.0) # Wait for ESP32 boot
        except Exception as e:
            log(f"Failed to open {target_port}: {e}")
            time.sleep(5)
            continue

        try:
            while True:
                if is_screen_locked():
                    # Don't send anything if locked, let ESP32 timeout and sleep
                    time.sleep(POLL_INTERVAL)
                    continue
                
                is_mus = is_audio_playing()
                app = get_active_app()
                is_work = any(x in app for x in ["vscode", "cursor", "xcode", "terminal", "iterm", "warp", "antigravity-ide"])
                is_meet = any(x in app for x in ["zoom", "teams", "webex", "meet"])
                is_batt, is_charg = get_battery_state()
                is_cpu = get_cpu_stressed()
                
                cmd = ""
                cmd += 'M' if is_mus else 'm'
                cmd += 'W' if is_work else 'w'
                cmd += 'Z' if is_meet else 'z'
                cmd += 'B' if is_batt else 'b'
                cmd += 'C' if is_cpu else 'c'
                cmd += 'H' if is_charg else 'h'
                cmd += '\n'
                
                ser.write(cmd.encode('ascii'))
                ser.flush()
                log(f"Sent State: {cmd.strip()} (App: {app})") # uncomment for extreme debug
                time.sleep(POLL_INTERVAL)
                
        except serial.SerialException:
            log("Connection lost. Reconnecting...")
            ser.close()
            time.sleep(2)
        except Exception as e:
            log(f"Unexpected error: {e}")
            time.sleep(5)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\\nExiting.")
