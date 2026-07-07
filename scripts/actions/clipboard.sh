#!/bin/bash
# clipboard.sh — Clipboard manager
# Rewritten to use ocws-clip C binary when available.

set -euo pipefail

OCWS_CLIP="${HOME}/.local/bin/ocws-clip"

# Wire to C binary for core operations
if [[ -x "$OCWS_CLIP" ]]; then
    exec "$OCWS_CLIP" "$@"
fi

# Bash fallback (when C binary not installed)
MODE="${1:-show}"
MAX_ITEMS=50

pass() { echo -e "\033[0;32m✓\033[0m $1"; }

show_history() {
    if command -v cliphist &>/dev/null; then
        cliphist list | head -n "$MAX_ITEMS"
    elif command -v wl-paste &>/dev/null; then
        wl-paste --list 2>/dev/null | head -n "$MAX_ITEMS"
    else
        echo "Clipboard history not available. Install cliphist or wl-clipboard"
        exit 1
    fi
}

select_and_paste() {
    command -v cliphist &>/dev/null || { show_history; exit 0; }
    local selected=""
    if command -v rofi &>/dev/null; then
        selected=$(cliphist list | rofi -dmenu -p "Clipboard")
    elif command -v wofi &>/dev/null; then
        selected=$(cliphist list | wofi --dmenu -p "Clipboard")
    elif command -v fzf &>/dev/null; then
        selected=$(cliphist list | fzf --prompt="Clipboard> ")
    fi
    [ -n "$selected" ] && echo "$selected" | cliphist decode | wl-copy && pass "Copied to clipboard"
}

case "$MODE" in
    show|list|history) show_history ;;
    pick|select|rofi) select_and_paste ;;
    clear|delete)
        command -v cliphist &>/dev/null && cliphist delete-all && pass "History cleared"
        command -v wl-copy &>/dev/null && wl-copy -c && pass "Clipboard cleared"
        ;;
    copy) shift; [ -n "${*:-}" ] && echo -n "$*" | wl-copy && pass "Copied: $*" ;;
    paste) command -v wl-paste &>/dev/null && wl-paste ;;
    *) echo "Usage: $0 [show|pick|clear|copy TEXT|paste]" ;;
esac
