#!/bin/bash
# ocws-cmdbar — Global Command Bar for OCWS
# Triggered by Super+F, exposes all available scripts and utilities
# Fuzzy-friendly format: "category/name"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR=""
_candidate="$SCRIPT_DIR"
while [[ "$_candidate" != "/" ]]; do
  if [[ -d "$_candidate/themes" ]]; then
    PROJECT_DIR="$_candidate"
    break
  fi
  _candidate="$(dirname "$_candidate")"
done

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${CYAN}→${NC} $1"; }
warn()  { echo -e "${YELLOW}⚠${NC} $1"; }

# ============================================================
# Command registry: display_name -> actual_command
# ============================================================

declare -A CMD_MAP

register() {
    CMD_MAP["$1"]="$2"
}

# System
register "system/status"         "ocws-sysmon"
register "system/monitor"        "ocws-sysmon"
register "system/health"         "$PROJECT_DIR/scripts/ocws-health.sh"
register "system/pkgmgr"         "ocws-pkgmgr"
register "system/settings"       "ocws-settings"
register "system/welcome"        "ocws-welcome"
register "system/validate"       "ocws-validate"

# Audio
register "audio/volume-up"       "$SCRIPT_DIR/audio.sh up 5%"
register "audio/volume-down"     "$SCRIPT_DIR/audio.sh down 5%"
register "audio/mute"            "$SCRIPT_DIR/audio.sh mute"
register "audio/settings"        "pavucontrol"

# Display
register "display/brightness-up"   "$SCRIPT_DIR/brightness.sh up 10%"
register "display/brightness-down" "$SCRIPT_DIR/brightness.sh down 10%"
register "display/settings"        "$PROJECT_DIR/scripts/ocws-display.sh"

# Windows
register "window/center"         "labwc:super+c"
register "window/float"          "labwc:super+space"
register "window/maximize"       "labwc:super+m"
register "window/fullscreen"     "labwc:super+f"
register "window/close"          "labwc:alt+q"
register "window/tiling"         "ocws-hypertile"

# Workspaces
register "workspace/1"           "labwc:super+1"
register "workspace/2"           "labwc:super+2"
register "workspace/3"           "labwc:super+3"
register "workspace/4"           "labwc:super+4"
register "workspace/manager"     "ocws-workspace-mgr"

# Tools
register "tool/launcher"         "rofi -show drun"
register "tool/run"              "rofi -show run"
register "tool/windows"          "rofi -show window"
register "tool/screenshot"       "$SCRIPT_DIR/screenshot.sh"
register "tool/ocr"              "ocws-ocr"
register "tool/clipboard"        "ocws-clip"
register "tool/color"            "ocws-color"
register "tool/search"           "ocws-search"
register "tool/player"           "ocws-player"
register "tool/recorder"         "ocws-recorder"
register "tool/lock"             "ocws-lock"
register "tool/calc"             "$SCRIPT_DIR/fuzzel-calc.sh"
register "tool/emoji"            "$SCRIPT_DIR/fuzzel-emoji.sh"

# Network
register "network/status"        "nmcli"
register "network/wifi"          "nmcli radio wifi"
register "network/bluetooth"     "blueman-manager"
register "network/vpn"           "nmcli connection show"

# Theme
register "theme/switcher"        "$PROJECT_DIR/scripts/theme-engine.sh list"
register "theme/wallpaper"       "wallpaper random"
register "theme/icons"           "$SCRIPT_DIR/icon-theme-picker.sh"
register "theme/font-scale"      "$PROJECT_DIR/scripts/font-scale.sh"

# Shell
register "shell/mode"            "$SCRIPT_DIR/shell-mode-picker.sh"
register "shell/reload"          "labwc:super+shift+c"
register "shell/configure"       "$PROJECT_DIR/scripts/ocws-configure.sh"
register "shell/dock-test"       "$PROJECT_DIR/scripts/sfwbar-dock-test.sh start"
register "shell/dock-stop"       "$PROJECT_DIR/scripts/sfwbar-dock-test.sh stop"

# Maintenance
register "maint/backup"          "$PROJECT_DIR/scripts/backup.sh"
register "maint/restore"         "$PROJECT_DIR/scripts/restore.sh"
register "maint/sync"            "$PROJECT_DIR/scripts/dotfiles-sync.sh"
register "maint/clean"           "$PROJECT_DIR/scripts/clean.sh"
register "maint/update"          "$PROJECT_DIR/scripts/update.sh"
register "maint/debug"           "$PROJECT_DIR/scripts/debug-labwc.sh"
register "maint/dock-save"       "$SCRIPT_DIR/dock-pin-backup.sh save"
register "maint/dock-load"       "$SCRIPT_DIR/dock-pin-backup.sh load"
register "maint/dock-list"       "$SCRIPT_DIR/dock-pin-backup.sh list"
register "maint/dock-mgr"        "ocws-dock-mgr"
register "maint/dock-test"       "$PROJECT_DIR/scripts/sfwbar-dock-test.sh start"
register "maint/dock-stop"       "$PROJECT_DIR/scripts/sfwbar-dock-test.sh stop"

# ============================================================
# Build sorted menu
# ============================================================

build_menu() {
    local keys=()
    for key in "${!CMD_MAP[@]}"; do
        keys+=("$key")
    done
    IFS=$'\n' keys=($(sort <<<"${keys[*]}")); unset IFS
    printf '%s\n' "${keys[@]}"
}

# ============================================================
# Execute action
# ============================================================

execute_action() {
    local choice="$1"
    local cmd="${CMD_MAP[$choice]:-}"

    [[ -z "$cmd" ]] && return

    # Handle labwc keybind hints
    if [[ "$cmd" == labwc:* ]]; then
        local key="${cmd#labwc:}"
        info "Use keybind: $key"
        return
    fi

    # Handle commands with arguments (space-separated)
    if [[ "$cmd" == *" "* ]]; then
        local bin="${cmd%% *}"
        local args="${cmd#* }"
        if [[ -f "$bin" ]]; then
            "$bin" $args &
        elif command -v "$bin" &>/dev/null; then
            "$bin" $args &
        else
            warn "$bin not found"
        fi
        return
    fi

    # Handle single commands
    if [[ -f "$cmd" ]]; then
        "$cmd" &
    elif command -v "$cmd" &>/dev/null; then
        "$cmd" &
    else
        warn "$cmd not found"
    fi
}

# ============================================================
# Rofi launcher
# ============================================================

show_rofi() {
    if ! command -v rofi &>/dev/null; then
        warn "rofi not found"
        exit 1
    fi

    local choice
    choice=$(build_menu | rofi -dmenu -p "❯ " -w 40 -l 30 \
        -theme-str 'window {width: 500px;}')

    [[ -z "$choice" ]] && exit 0

    execute_action "$choice"
}

# ============================================================
# Fuzzel launcher (alternative)
# ============================================================

show_fuzzel() {
    if ! command -v fuzzel &>/dev/null; then
        warn "fuzzel not found"
        exit 1
    fi

    local choice
    choice=$(build_menu | fuzzel --dmenu -p "❯ " -w 40 -l 30)

    [[ -z "$choice" ]] && exit 0

    execute_action "$choice"
}

# ============================================================
# Main
# ============================================================

MODE="${1:-}"

case "$MODE" in
    ""|menu|show)
        if command -v rofi &>/dev/null; then
            show_rofi
        elif command -v fuzzel &>/dev/null; then
            show_fuzzel
        else
            echo -e "${BOLD}Available Commands:${NC}"
            build_menu
        fi
        ;;
    list|ls)
        build_menu
        ;;
    *)
        # Direct execution: ocws-cmdbar tool/screenshot
        if [[ -n "${CMD_MAP[$MODE]:-}" ]]; then
            execute_action "$MODE"
        else
            warn "Unknown: $MODE"
            echo "Usage: ocws-cmdbar [category/name]"
            echo ""
            echo "Examples:"
            echo "  ocws-cmdbar tool/screenshot"
            echo "  ocws-cmdbar audio/mute"
            echo "  ocws-cmdbar theme/switcher"
        fi
        ;;
esac
