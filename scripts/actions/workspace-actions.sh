#!/bin/bash
# workspace-actions.sh — Simple Superkey+f launcher
# Flat structure: Category > Action format

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR=""

# Find project root (scripts/actions is 2 levels deep)
if [[ -d "$(dirname "$(dirname "$SCRIPT_DIR")")/themes" ]]; then
    PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
else
    # Fallback if moved, assume script is run from project root or something
    PROJECT_DIR="$PWD"
fi

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

info() { echo -e "${CYAN}→${NC} $1"; }
warn() { echo -e "${YELLOW}⚠${NC} $1"; }
fail() { echo -e "${RED}✗${NC} $1"; exit 1; }

# Display flat rofi menu
show_rofi_menu() {
    if ! command -v rofi >/dev/null 2>&1; then
        warn "Rofi not found, cannot show interactive menu."
        exit 1
    fi

    local options=(
        "Audio > Volume Up"
        "Audio > Volume Down"
        "Audio > Mute Toggle"
        "Display > Brightness Up"
        "Display > Brightness Down"
        "Display > Auto-Rotate"
        "Windows > Center Window"
        "Windows > Focus Floating"
        "Windows > Next Workspace"
        "Windows > Workspace 1"
        "Windows > Workspace 2"
        "System > System Status"
        "System > File Manager"
        "System > Terminal"
        "System > Web Browser"
        "Tools > App Launcher"
        "Tools > Run Command"
        "Tools > Screenshot"
        "Tools > Lock Screen"
        "Tools > Theme Switcher"
        "Network > Network Status"
        "Network > WiFi Toggle"
        "Network > Bluetooth Toggle"
        "Settings > Quick Settings"
    )

    local choice=$(printf '%s\n' "${options[@]}" | rofi -dmenu -p "Action ❯ " -w 40 -l 15)

    case "$choice" in
        "Audio > Volume Up") execute_action "volume-up" ;;
        "Audio > Volume Down") execute_action "volume-down" ;;
        "Audio > Mute Toggle") execute_action "mute" ;;
        "Display > Brightness Up") execute_action "brightness-up" ;;
        "Display > Brightness Down") execute_action "brightness-down" ;;
        "Display > Auto-Rotate") execute_action "auto-rotate" ;;
        "Windows > Center Window") execute_action "center" ;;
        "Windows > Focus Floating") execute_action "floating" ;;
        "Windows > Next Workspace") execute_action "next-workspace" ;;
        "Windows > Workspace 1") execute_action "workspace-1" ;;
        "Windows > Workspace 2") execute_action "workspace-2" ;;
        "System > System Status") execute_action "status" ;;
        "System > File Manager") execute_action "file" ;;
        "System > Terminal") execute_action "terminal" ;;
        "System > Web Browser") execute_action "browser" ;;
        "Tools > App Launcher") execute_action "launcher" ;;
        "Tools > Run Command") execute_action "run-cmd" ;;
        "Tools > Screenshot") execute_action "screenshot" ;;
        "Tools > Lock Screen") execute_action "lock" ;;
        "Tools > Theme Switcher") execute_action "theme" ;;
        "Network > Network Status") execute_action "network" ;;
        "Network > WiFi Toggle") execute_action "wifi" ;;
        "Network > Bluetooth Toggle") execute_action "bt" ;;
        "Settings > Quick Settings") execute_action "settings" ;;
        *) exit 0 ;;
    esac
}

# Execute action
execute_action() {
    local action="$1"
    local ACTIONS_DIR
    ACTIONS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    
    case "$action" in
        dock|dashboard|menu|dock-menu|app-dock)
            # Launch enhanced dock with app pinning and task management
            if command -v sh >/dev/null 2>&1; then
                "$ACTIONS_DIR/dock.sh"
            else
                info "Dock not available"
            fi
            ;;
        # Audio Controls
        up|volume-up|vol-up|add-volume)
            "$ACTIONS_DIR/audio.sh" up 5%
            ;;
        down|volume-down|vol-down|sub-volume)
            "$ACTIONS_DIR/audio.sh" down 5%
            ;;
        mute|toggle-mute|mute-audio)
            "$ACTIONS_DIR/audio.sh" mute
            ;;
        vol-up-0.5|volume-up-0.5|vol-up-half)
            "$ACTIONS_DIR/audio.sh" up-0.5
            ;;
        vol-down-0.5|volume-down-0.5|vol-down-half)
            "$ACTIONS_DIR/audio.sh" down-0.5
            ;;
        
        # Brightness Controls
        bright-up|brightness-up|inc-bright)
            "$ACTIONS_DIR/brightness.sh" up 10%
            ;;
        bright-down|brightness-down|dec-bright)
            "$ACTIONS_DIR/brightness.sh" down 10%
            ;;
        bright-up-0.5|brightness-up-0.5)
            "$ACTIONS_DIR/brightness.sh" up-0.5
            ;;
        bright-down-0.5|brightness-down-0.5)
            "$ACTIONS_DIR/brightness.sh" down-0.5
            ;;
        auto-rotate|rotate)
            # Auto-rotate toggle (placeholder)
            info "Auto-rotate display toggle"
            ;;
        
        # Window Management
        center|center-window|zentern)
            xdotool key --clearmodifiers super+c
            ;;
        floating|focus-floating|focus-float)
            xdotool key --clearmodifiers super+f
            ;;
        next-workspace|ws-next|super-tab)
            xdotool key --clearmodifiers super+Tab
            ;;
        ws|workspace|workspaces)
            xdotool key --clearmodifiers super+space
            ;;
        ws1|workspace-1|super-1)
            xdotool key --clearmodifiers super+1
            ;;
        ws2|workspace-2|super-2)
            xdotool key --clearmodifiers super+2
            ;;
        ws9|workspace-9|super-9)
            xdotool key --clearmodifiers super+9
            ;;
        
        # System Services
        system|status|sysinfo|sys)
            "$ACTIONS_DIR/system-info.sh"
            ;;
        file|files|file-manager)
            xdg-open ~ 2>/dev/null || nautilus ~ 2>/dev/null || dolphin ~ 2>/dev/null
            ;;
        term|terminal|shell|bash)
            if command -v foot >/dev/null; then
                foot &
            elif command -v kitty >/dev/null; then
                kitty &
            elif command -v gnome-terminal >/dev/null; then
                gnome-terminal &
            else
                info "No terminal found"
            fi
            ;;
        browser|web|surf|www)
            if command -v chromium >/dev/null; then
                chromium &
            elif command -v firefox >/dev/null; then
                firefox &
            elif command -v brave >/dev/null; then
                brave &
            else
                info "No browser found"
            fi
            ;;
        quit|exit|close)
            # Generic quit action (placeholder - user should use window manager shortcuts)
            info "Use window manager shortcut (e.g., Super+q)"
            ;;
        
        # Quick Tools
        launcher|search|find|run)
            if command -v rofi >/dev/null; then
                rofi -show drun
            else
                info "Launcher not available"
            fi
            ;;
        run-cmd|run-command|execute)
            if command -v rofi >/dev/null; then
                rofi -show run
            else
                "$ACTIONS_DIR/launcher.sh" run "$@"
            fi
            ;;
        screenshot|capture|screen)
            "$ACTIONS_DIR/screenshot.sh"
            ;;
        lock|lock-screen)
            if command -v loginctl >/dev/null; then
                loginctl lock-session
            else
                xset s activate
            fi
            ;;
        theme|theme-switch|colors)
            local SCRIPTS_DIR
            SCRIPTS_DIR="$(dirname "$ACTIONS_DIR")"
            "$SCRIPTS_DIR/theme-engine.sh" list
            if command -v rofi >/dev/null; then
                selected=$("$SCRIPTS_DIR/theme-engine.sh" list | rofi -dmenu -p "🎨 Select Theme: " -theme-str 'window {width: 400px;}')
                if [[ -n "$selected" ]]; then
                    "$SCRIPTS_DIR/theme-engine.sh" apply "themes/${selected}.ini"
                fi
            fi
            ;;
        
        # Communication & Network
        network|net)
            "$ACTIONS_DIR/network.sh" status
            ;;
        wifi|wireless)
            "$ACTIONS_DIR/network.sh" wifi-toggle
            ;;
        bt|bluetooth)
            "$ACTIONS_DIR/network.sh" bt-toggle
            ;;
        
        # System Settings
        settings|prefs|config)
            if command -v rofi >/dev/null; then
                rofi -show drun -theme-str 'window {width: 600px;}'
            else
                info "Open settings from system menu"
            fi
            ;;
        help|about|info|manual)
            show_rofi_menu
            ;;
        
        # Default action
        *)
            warn "Unknown action: $action"
            show_rofi_menu
            ;;
    esac
}

# Execute based on argument
MODE="${1:-list}"
if [ $# -gt 0 ]; then
    shift
fi

# Main execution logic
case "$MODE" in
    list|ls|help|--help|-h)
        show_rofi_menu
        ;;
    *)
        execute_action "$MODE"
        ;;
esac
