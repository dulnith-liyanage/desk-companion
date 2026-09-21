#!/bin/bash
# ============================================================================
# Desk Companion - Music Monitor Uninstall (macOS)
# ============================================================================
# Removes the Launch Agent and optionally the virtual environment.
#
# Usage:
#   chmod +x uninstall.sh
#   ./uninstall.sh
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLIST_LABEL="com.deskcompanion.musicmonitor"
PLIST_DEST="$HOME/Library/LaunchAgents/${PLIST_LABEL}.plist"
VENV_DIR="$SCRIPT_DIR/venv"

echo "============================================"
echo "  Desk Companion - Music Monitor Uninstall"
echo "============================================"
echo ""

# 1. Unload and remove Launch Agent
if [ -f "$PLIST_DEST" ]; then
    echo "[1/2] Stopping and removing Launch Agent..."
    launchctl unload "$PLIST_DEST" 2>/dev/null || true
    rm "$PLIST_DEST"
    echo "  → Launch Agent removed ✓"
else
    echo "[1/2] No Launch Agent found (already removed)"
fi

# 2. Optionally remove venv
echo ""
if [ -d "$VENV_DIR" ]; then
    read -p "[2/2] Remove Python virtual environment at $VENV_DIR? (y/N) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$VENV_DIR"
        echo "  → Virtual environment removed ✓"
    else
        echo "  → Virtual environment kept"
    fi
else
    echo "[2/2] No virtual environment found"
fi

echo ""
echo "============================================"
echo "  ✅ Uninstall complete!"
echo "============================================"
echo ""
echo "The music monitor will no longer start on login."
echo "To reinstall, run: ./setup.sh"
echo ""
