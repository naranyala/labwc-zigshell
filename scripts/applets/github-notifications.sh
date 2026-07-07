#!/bin/bash
set -euo pipefail

# GitHub Notifications Prototype
# Fetches unread GitHub notifications count and emits to the Event Bus
# C-rewrite goal: Same unified HTTP fetcher (libcurl) with auth handling

# Configuration
INTERVAL=600 # 10 minutes

# Assumes GH_TOKEN is set in environment or in a config file
if [[ -f "$HOME/.config/ocws/tokens/github" ]]; then
    GH_TOKEN=$(cat "$HOME/.config/ocws/tokens/github")
fi

EMIT_CMD="ocws-emit"
if ! command -v $EMIT_CMD &> /dev/null; then
    EMIT_CMD="sh ../ocws-emit.sh" # fallback
fi

log() { echo "[$(date '+%H:%M:%S')] [GitHub] $*"; }

fetch_notifications() {
    if [[ -z "${GH_TOKEN:-}" ]]; then
        # No token, gracefully emit 0 or "Auth" to indicate needs setup
        $EMIT_CMD Applet.GitHubUnread "?"
        return 0
    fi

    log "Fetching GitHub notifications..."
    local url="https://api.github.com/notifications"
    
    local response
    # We just want the count, we can use jq length
    if ! response=$(curl -s --max-time 10 -H "Authorization: token $GH_TOKEN" "$url"); then
        log "Error: Failed to fetch API"
        return 1
    fi
    
    # Check if we got an unauthorized error or a valid array
    if echo "$response" | grep -q '"message": "Bad credentials"'; then
        log "Error: Bad token"
        $EMIT_CMD Applet.GitHubUnread "X"
        return 1
    fi

    local unread_count=$(echo "$response" | jq 'length' 2>/dev/null || echo "0")
    
    $EMIT_CMD Applet.GitHubUnread "$unread_count"
    log "Unread: $unread_count"
}

while true; do
    fetch_notifications || true
    sleep $INTERVAL
done
