#!/bin/bash
# mic.sh — Microphone control (mute toggle, volume, list sources)
# Rewritten to use ocws-volume C binary for core operations.

set -euo pipefail

OCWS_VOLUME="${HOME}/.local/bin/ocws-volume"
SRC_DEVICE="@DEFAULT_SOURCE@"

# Wire to C binary for core operations
if [[ -x "$OCWS_VOLUME" ]]; then
    exec "$OCWS_VOLUME" --device "$SRC_DEVICE" "$@"
fi

# Bash fallback (when C binary not installed)
MODE="${1:-toggle}"

notify() {
    command -v notify-send &>/dev/null && notify-send -a "Microphone" -t 2000 "$1"
}

case "$MODE" in
    toggle|mute) wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle 2>/dev/null && notify "Mic toggled" ;;
    unmute) wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0 2>/dev/null && notify "Mic unmuted" ;;
    status)
        RAW=$(wpctl get-volume @DEFAULT_AUDIO_SOURCE@ 2>/dev/null || echo "Volume: 0.00")
        if echo "$RAW" | grep -q "MUTED"; then echo "Microphone: muted"
        else echo "Microphone: active"; fi
        ;;
    list) wpctl status 2>/dev/null | grep -A 50 "Sources:" ;;
    *) echo "Usage: $0 [toggle|mute|unmute|status|list]" ;;
esac
