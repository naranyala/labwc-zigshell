#!/bin/bash
# screenshot.sh — Screenshot with annotation
# Rewritten to use ocws-shot C binary when available.

set -euo pipefail

OCWS_SHOT="${HOME}/.local/bin/ocws-shot"

# Wire to C binary for core operations
if [[ -x "$OCWS_SHOT" ]]; then
    exec "$OCWS_SHOT" "$@"
fi

# Bash fallback (when C binary not installed)
MODE="${1:-menu}"
DELAY="${2:-0}"
SAVE_DIR="${HOME}/Pictures/screenshots"
CLIPBOARD=true

mkdir -p "$SAVE_DIR"

pass() { echo -e "\033[0;32m✓\033[0m $1"; }
fail() { echo -e "\033[0;31m✗\033[0m $1"; exit 1; }

FILENAME="screenshot-$(date +%Y%m%d-%H%M%S).png"
FILEPATH="$SAVE_DIR/$FILENAME"

take_screenshot() {
    if command -v grim &>/dev/null && command -v slurp &>/dev/null; then
        case "$MODE" in
            full) grim "$FILEPATH" ;;
            area) grim -g "$(slurp)" "$FILEPATH" ;;
            window) grim -g "$(slurp -w)" "$FILEPATH" ;;
            timer) sleep "$DELAY"; grim "$FILEPATH" ;;
            annotate)
                if command -v satty &>/dev/null; then
                    grim -g "$(slurp)" - | satty - --save-file "$FILEPATH" --copy-to-clipboard
                    return
                elif command -v swappy &>/dev/null; then
                    grim -g "$(slurp)" - | swappy -f - -o "$FILEPATH"
                    return
                else
                    grim -g "$(slurp)" "$FILEPATH"
                fi ;;
            annotate-full)
                if command -v satty &>/dev/null; then
                    grim - | satty - --save-file "$FILEPATH" --copy-to-clipboard
                    return
                elif command -v swappy &>/dev/null; then
                    grim - | swappy -f - -o "$FILEPATH"
                    return
                else
                    grim "$FILEPATH"
                fi ;;
        esac
    elif command -v flameshot &>/dev/null; then
        case "$MODE" in
            full) flameshot full -p "$SAVE_DIR" ;;
            area|window|annotate) flameshot gui -p "$SAVE_DIR" ;;
            timer) flameshot full -d "$((DELAY * 1000))" -p "$SAVE_DIR" ;;
        esac
    elif command -v ksnip &>/dev/null; then
        case "$MODE" in
            full) ksnip -f "$FILEPATH" ;;
            area|annotate) ksnip -r ;;
            window) ksnip -a ;;
            timer) sleep "$DELAY" && ksnip -f "$FILEPATH" ;;
        esac
    else
        fail "No screenshot tool found. Install grim+slurp, flameshot, or ksnip"
    fi
}

show_menu() {
    local opts=("Area" "Full Screen" "Active Window" "Area + Annotate" "Full + Annotate" "Timer (3s)")
    if command -v rofi &>/dev/null; then
        choice=$(printf '%s\n' "${opts[@]}" | rofi -dmenu -p "Screenshot")
    else
        echo "Select mode:"; select choice in "${opts[@]}"; do break; done
    fi
    case "$choice" in
        *"Area"*) MODE="area" ;; *"Full"*) MODE="full" ;; *"Window"*) MODE="window" ;;
        *"Annotate"*) MODE="annotate" ;; *"Timer"*) MODE="timer"; DELAY=3 ;; *) exit 0 ;;
    esac
    take_screenshot
}

if [[ "$MODE" == "menu" ]]; then show_menu; else take_screenshot; fi

if $CLIPBOARD && [ -f "$FILEPATH" ] && [[ "$MODE" != annotate* ]]; then
    command -v wl-copy &>/dev/null && wl-copy < "$FILEPATH"
    pass "Copied to clipboard"
fi
[ -f "$FILEPATH" ] && pass "Saved: $FILEPATH"
