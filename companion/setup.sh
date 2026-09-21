#!/bin/bash
# ============================================================================
# Desk Companion - Music Monitor Setup (macOS)
# ============================================================================
# One-time setup script that:
#   1. Creates a Python virtual environment and installs dependencies
#   2. Installs a macOS Launch Agent to auto-start the music monitor on login
#
# After running this script, the music monitor will start automatically
# whenever you log in. The ESP32 will show the music face whenever
# Spotify, Apple Music, or browser audio is playing.
#
# Usage:
#   chmod +x setup.sh
#   ./setup.sh
#
# To uninstall:
#   ./uninstall.sh
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLIST_LABEL="com.deskcompanion.musicmonitor"
PLIST_DEST="$HOME/Library/LaunchAgents/${PLIST_LABEL}.plist"
VENV_DIR="$SCRIPT_DIR/venv"
MONITOR_SCRIPT="$SCRIPT_DIR/music_monitor.py"
LOG_DIR="$HOME/Library/Logs/DeskCompanion"

echo "============================================"
echo "  Desk Companion - Music Monitor Setup"
echo "============================================"
echo ""

# 1. Create virtual environment
echo "[1/3] Setting up Python virtual environment..."
if [ -d "$VENV_DIR" ]; then
    echo "  → venv already exists at $VENV_DIR"
else
    python3 -m venv "$VENV_DIR"
    echo "  → Created venv at $VENV_DIR"
fi

# Install dependencies
echo "  → Installing dependencies..."
"$VENV_DIR/bin/pip" install --quiet --upgrade pip
"$VENV_DIR/bin/pip" install --quiet -r "$SCRIPT_DIR/requirements.txt"
echo "  → Dependencies installed ✓"
echo ""

# 2. Create log directory
echo "[2/3] Creating log directory..."
mkdir -p "$LOG_DIR"
echo "  → Logs will be at $LOG_DIR"
echo ""

# 3. Install Launch Agent
echo "[3/3] Installing macOS Launch Agent..."

# Unload existing agent if present
if launchctl list "$PLIST_LABEL" &>/dev/null; then
    echo "  → Unloading existing agent..."
    launchctl unload "$PLIST_DEST" 2>/dev/null || true
fi

# Generate the plist with correct absolute paths
cat > "$PLIST_DEST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>${PLIST_LABEL}</string>

    <key>ProgramArguments</key>
    <array>
        <string>${VENV_DIR}/bin/python</string>
        <string>${MONITOR_SCRIPT}</string>
    </array>

    <key>WorkingDirectory</key>
    <string>${SCRIPT_DIR}</string>

    <key>RunAtLoad</key>
    <true/>

    <key>KeepAlive</key>
    <dict>
        <!-- Restart if it crashes, but not if it exits cleanly (e.g. uninstall) -->
        <key>SuccessfulExit</key>
        <false/>
    </dict>

    <key>ThrottleInterval</key>
    <integer>10</integer>

    <key>StandardOutPath</key>
    <string>${LOG_DIR}/music_monitor.log</string>

    <key>StandardErrorPath</key>
    <string>${LOG_DIR}/music_monitor_error.log</string>

    <key>ProcessType</key>
    <string>Background</string>
</dict>
</plist>
EOF

echo "  → Installed plist at $PLIST_DEST"

# Load the agent
launchctl load "$PLIST_DEST"
echo "  → Launch Agent loaded and running ✓"

echo ""
echo "============================================"
echo "  ✅ Setup complete!"
echo "============================================"
echo ""
echo "The music monitor is now running in the background."
echo "It will auto-start on every login."
echo ""
echo "  Logs:      $LOG_DIR/music_monitor.log"
echo "  Errors:    $LOG_DIR/music_monitor_error.log"
echo "  Status:    launchctl list | grep deskcompanion"
echo "  Stop:      launchctl unload $PLIST_DEST"
echo "  Restart:   launchctl unload $PLIST_DEST && launchctl load $PLIST_DEST"
echo "  Uninstall: ./uninstall.sh"
echo ""
