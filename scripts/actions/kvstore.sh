#!/bin/bash
# kvstore.sh — Persistent key-value store for OCWS
# Rewritten to use ocws-kv C binary for core operations.

OCWS_DIR="${OCWS_DIR:-$HOME/.config/ocws}"
KVSTORE_FILE="$OCWS_DIR/state/kvstore"
OCWS_KV="${HOME}/.local/bin/ocws-kv"

mkdir -p "$(dirname "$KVSTORE_FILE")"

# Wire to C binary for core CRUD operations
if [[ -x "$OCWS_KV" ]]; then
    CMD="${1:-}"
    case "$CMD" in
        set|get|del|list)
            exec "$OCWS_KV" "$@"
            ;;
        export|import)
            # C binary uses set/get — map export/import
            exec "$OCWS_KV" set "$2" "$3"
            ;;
        retrieve)
            exec "$OCWS_KV" get "$2"
            ;;
        delete)
            exec "$OCWS_KV" del "$2"
            ;;
    esac
    # Fall through for commands C binary doesn't support
fi

# Bash fallback for advanced commands (backup, restore, merge, json, export-commands)
case "${1:-help}" in
    set|export|import) echo "$2=$3" >> "$KVSTORE_FILE" ;;
    get|retrieve) grep "^$2=" "$KVSTORE_FILE" 2>/dev/null | cut -d= -f2- ;;
    del|delete) grep -v "^$2=" "$KVSTORE_FILE" > "${KVSTORE_FILE}.tmp" 2>/dev/null && mv "${KVSTORE_FILE}.tmp" "$KVSTORE_FILE" ;;
    list) cat "$KVSTORE_FILE" 2>/dev/null ;;
    flush) rm -f "$KVSTORE_FILE" ;;
    find) grep -i "$2" "$KVSTORE_FILE" 2>/dev/null ;;
    json)
        echo "{"; first=true
        while IFS='=' read -r k v; do
            $first || echo ","; first=false
            printf '  "%s": "%s"' "$k" "${v//\"/\\\"}"
        done < "$KVSTORE_FILE" 2>/dev/null
        echo ""; echo "}" ;;
    backup)
        bd="$OCWS_DIR/state/backups/$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$bd" && cp "$KVSTORE_FILE" "$bd/kvstore" && echo "Backed up: $bd/kvstore" ;;
    restore) [ -f "${2:-}" ] && cp "$2" "$KVSTORE_FILE" && echo "Restored: $2" ;;
    merge) [ -f "${2:-}" ] && cat "$KVSTORE_FILE" "$2" | sort -u > "$KVSTORE_FILE" && echo "Merged: $2" ;;
    *) echo "Usage: $0 [set|get|del|list|flush|find|json|backup|restore|merge] [args]" ;;
esac
