#!/bin/bash
set -euo pipefail

# Pomodoro Timer Prototype
# Uses ocws-state to save timer state and ocws-emit to update UI
# C-rewrite goal: Integrate into ocws-brokerd timer subsystem

EMIT_CMD="ocws-emit"
STATE_FILE="/tmp/ocws-pomodoro.state" # Temporary, eventually ocws-kv

log() { echo "[$(date '+%H:%M:%S')] [Pomodoro] $*"; }

if [[ "${1:-}" == "start" ]]; then
    duration_min=${2:-25}
    end_time=$(( $(date +%s) + (duration_min * 60) ))
    echo "$end_time" > "$STATE_FILE"
    $EMIT_CMD Applet.PomodoroState "running"
    log "Started $duration_min minute timer"
    exit 0
elif [[ "${1:-}" == "stop" ]]; then
    rm -f "$STATE_FILE"
    $EMIT_CMD Applet.PomodoroState "stopped"
    $EMIT_CMD Applet.PomodoroTime "00:00"
    log "Stopped timer"
    exit 0
fi

# If no args, run daemon loop
while true; do
    if [[ -f "$STATE_FILE" ]]; then
        end_time=$(cat "$STATE_FILE")
        now=$(date +%s)
        
        if (( now >= end_time )); then
            # Timer finished!
            rm -f "$STATE_FILE"
            $EMIT_CMD Applet.PomodoroState "finished"
            $EMIT_CMD Applet.PomodoroTime "00:00"
            log "Timer finished, emitting notification..."
            
            # Use native notification if available
            if command -v ocws-notify &> /dev/null; then
                ocws-notify --app "Pomodoro" --title "Time's Up!" --body "Take a break!" --icon "timer"
            else
                notify-send "Pomodoro" "Time's up! Take a break."
            fi
        else
            # Calculate remaining time
            rem=$(( end_time - now ))
            mins=$(( rem / 60 ))
            secs=$(( rem % 60 ))
            time_str=$(printf "%02d:%02d" $mins $secs)
            
            $EMIT_CMD Applet.PomodoroTime "$time_str"
            $EMIT_CMD Applet.PomodoroState "running"
        fi
    else
        # Emit resting state occasionally just to be safe
        $EMIT_CMD Applet.PomodoroState "stopped"
        $EMIT_CMD Applet.PomodoroTime "25:00"
    fi
    sleep 1
done
