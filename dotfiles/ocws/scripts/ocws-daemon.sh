#!/bin/bash
# -------------------------------------------------------------------
# OCWS State Daemon (Phase 3)
# Event-driven IPC for sfwbar - No more polling!
# -------------------------------------------------------------------

set -euo pipefail

# Stop any previously running instances, excluding this process
_MY_PID=$$
pgrep -f "ocws-daemon.sh" 2>/dev/null | grep -v "^${_MY_PID}$" | xargs -r kill 2>/dev/null || true
sleep 0.2

# Remove previous state files
rm -f /tmp/ocws-state /tmp/ocws-current-song

update_volume() {
    RAW=$(wpctl get-volume @DEFAULT_SINK@ 2>/dev/null || echo "Volume: 0.00")
    VOL=$(echo "$RAW" | grep -oP '(?<=Volume: )[0-9.]+' || echo "0.00")
    VOL_PERCENT=$(echo "$VOL * 100 / 1" | bc 2>/dev/null || echo "0")
    
    MUTED=0
    if echo "$RAW" | grep -q "MUTED"; then MUTED=1; fi
    
    # Push to sfwbar if running (silent fail if not)
    ~/.local/bin/ocws-emit.sh System.Volume "$VOL_PERCENT" 2>/dev/null || true
    ~/.local/bin/ocws-emit.sh System.VolumeMuted "$MUTED" 2>/dev/null || true
    
    # Write to state file for other consumers
    echo "VOLUME=$VOL_PERCENT" > /tmp/ocws-state-volume
    echo "MUTED=$MUTED" >> /tmp/ocws-state-volume
}

update_brightness() {
    BRIGHT=$(brightnessctl -m 2>/dev/null | cut -d, -f4 | tr -d % || echo 100)
    # Push to sfwbar if running (silent fail if not)
    ~/.local/bin/ocws-emit.sh System.Brightness "$BRIGHT" 2>/dev/null || true
    
    # Write to state file
    echo "BRIGHTNESS=$BRIGHT" > /tmp/ocws-state-brightness
}

# 1. Volume Listener
if command -v pactl >/dev/null 2>&1; then
    (
        # Listen for PipeWire/PulseAudio sink changes
        pactl subscribe 2>/dev/null | grep --line-buffered "Event 'change' on sink" | while read -r line; do
            update_volume
        done
    ) &
fi

# 2. Brightness Listener
if command -v inotifywait >/dev/null 2>&1; then
    (
        # Listen for backlight file changes directly from kernel
        inotifywait -m -e modify /sys/class/backlight/*/brightness 2>/dev/null | while read -r line; do
            update_brightness
        done
    ) &
fi

  # 3. Media Art Listener
if command -v playerctl >/dev/null 2>&1; then
    (
        while true; do
            playerctl metadata -F mpris:artUrl 2>/dev/null | while read -r ART_URL; do
                # Only process URLs and handle download errors gracefully
                if [[ "$ART_URL" == file://* ]]; then
                    if cp "${ART_URL#file://}" /tmp/ocws-cover.jpg 2>/dev/null; then
                        echo "Media art downloaded: $ART_URL" >&2
                    fi
                elif [[ "$ART_URL" == http* ]]; then
                    # Download with timeout and retry limit
                    if curl -sSL --max-time 10 --connect-timeout 5 "$ART_URL" -o /tmp/ocws-cover.jpg 2>/dev/null && [[ -f /tmp/ocws-cover.jpg ]]; then
                        echo "Media art downloaded: $ART_URL" >&2
                    else
                        rm -f /tmp/ocws-cover.jpg >&2
                    fi
                else
                    rm -f /tmp/ocws-cover.jpg 2>/dev/null
                fi
            done
            sleep 60
        done
    ) &
fi

wait
