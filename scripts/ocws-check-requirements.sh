#!/bin/bash
# ocws-check-requirements.sh — Pre-install requirements checker
# Shows users exactly what they need before running install.sh

set -euo pipefail

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# Counters
PASS=0
WARN=0
FAIL=0

check() {
    local label="$1" cmd="${2:-$1}"
    if command -v "$cmd" &>/dev/null; then
        echo -e "  ${GREEN}✓${NC} $label"
        PASS=$((PASS+1))
        return 0
    else
        echo -e "  ${RED}✗${NC} $label"
        FAIL=$((FAIL+1))
        return 1
    fi
}

check_optional() {
    local label="$1" cmd="${2:-$1}"
    if command -v "$cmd" &>/dev/null; then
        echo -e "  ${GREEN}✓${NC} $label"
        PASS=$((PASS+1))
    else
        echo -e "  ${YELLOW}○${NC} $label ${DIM}(optional)${NC}"
        WARN=$((WARN+1))
    fi
}

check_lib() {
    local label="$1" pkg="$2"
    if pkg-config --exists "$pkg" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} $label"
        PASS=$((PASS+1))
    else
        echo -e "  ${YELLOW}○${NC} $label ${DIM}(needed for building)${NC}"
        WARN=$((WARN+1))
    fi
}

# ============================================================
# Banner
# ============================================================

echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║${NC}  ${CYAN}OCWS${NC} — Our C-Written Shell                          ${BOLD}║${NC}"
echo -e "${BOLD}║${NC}  Pure C-native Wayland desktop environment              ${BOLD}║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================
# Detect distro
# ============================================================

DISTRO="unknown"
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID:-unknown}"
fi

echo -e "${BOLD}Detected: ${CYAN}${PRETTY_NAME:-$DISTRO}${NC}"
echo ""

# ============================================================
# Required: Core Compositor & Shell
# ============================================================

echo -e "${BOLD}[1/4] Core Desktop (required)${NC}"
HAS_LABWC=false
HAS_SFWBAR=false
HAS_FUZZEL=false

check "labwc (Wayland compositor)" "labwc" && HAS_LABWC=true
check "sfwbar (status bar)" "sfwbar" && HAS_SFWBAR=true
check "fuzzel (app launcher)" "fuzzel" && HAS_FUZZEL=true
check "foot (terminal)" "foot"
echo ""

# ============================================================
# Required: Runtime Tools
# ============================================================

echo -e "${BOLD}[2/4] Runtime Tools (required)${NC}"
check "playerctl (media control)" "playerctl"
check "grim (screenshot)" "grim"
check "slurp (area select)" "slurp"
check "wl-copy (clipboard)" "wl-copy"
check "brightnessctl" "brightnessctl"
check "jq (JSON)" "jq"
check "inotifywait (file watcher)" "inotifywait"
check "swaybg (wallpaper)" "swaybg"
echo ""

# ============================================================
# Optional: Build Tools
# ============================================================

echo -e "${BOLD}[3/4] Build Tools (optional — for C binary compilation)${NC}"
check_optional "zig (build system)" "zig"
check_optional "pkg-config" "pkg-config"
check_optional "git" "git"
echo ""

# ============================================================
# Optional: Nice-to-have
# ============================================================

echo -e "${BOLD}[4/4] Optional Extras${NC}"
check_optional "rofi (alternative launcher)" "rofi"
check_optional "mako (notifications)" "mako"
check_optional "cliphist (clipboard history)" "cliphist"
check_optional "qt6ct (Qt theming)" "qt6ct"
check_optional "tesseract (OCR)" "tesseract"
echo ""

# ============================================================
# Summary
# ============================================================

echo -e "${BOLD}══════════════════════════════════════════════════════════${NC}"
echo -e "  ${GREEN}PASS: $PASS${NC}  ${YELLOW}OPTIONAL: $WARN${NC}  ${RED}MISSING: $FAIL${NC}"
echo ""

if [[ $FAIL -gt 0 ]]; then
    echo -e "${RED}Some required dependencies are missing.${NC}"
    echo ""
    echo -e "${BOLD}Quick install command for your distro:${NC}"
    echo ""

    case "$DISTRO" in
        arch|manjaro|endeavouros|garuda)
            echo -e "  ${GREEN}sudo pacman -S labwc sfwbar fuzzel foot playerctl grim slurp wl-clipboard brightnessctl jq inotify-tools swaybg swayidle mako cliphist qt6ct${NC}"
            ;;
        debian|ubuntu|linuxmint|pop)
            echo -e "  ${GREEN}sudo apt install labwc sfwbar fuzzel foot playerctl grim slurp wl-clipboard brightnessctl jq inotify-tools swaybg swayidle mako-notifier cliphist qt6ct libgtk-3-dev${NC}"
            ;;
        fedora)
            echo -e "  ${GREEN}sudo dnf install labwc sfwbar fuzzel foot playerctl grim slurp wl-clipboard brightnessctl jq inotify-tools swaybg swayidle mako cliphist qt6ct gtk3-devel${NC}"
            ;;
        opensuse*|suse)
            echo -e "  ${GREEN}sudo zypper install labwc sfwbar fuzzel foot playerctl grim slurp wl-clipboard brightnessctl jq inotify-tools swaybg swayidle mako cliphist qt6ct gtk3-devel${NC}"
            ;;
        alpine)
            echo -e "  ${GREEN}sudo apk add labwc sfwbar rofi-wayland foot mako qt6ct fuzzel playerctl wl-clipboard cliphist grim slurp jq brightnessctl inotify-tools swaybg swayidle${NC}"
            ;;
        void)
            echo -e "  ${GREEN}sudo xbps-install -S labwc sfwbar rofi-wayland foot mako qt6ct fuzzel playerctl wl-clipboard cliphist grim slurp jq brightnessctl inotify-tools swaybg swayidle${NC}"
            ;;
        *)
            echo -e "  ${YELLOW}Unknown distro. Install these packages:${NC}"
            echo "    labwc sfwbar fuzzel foot playerctl grim slurp wl-clipboard"
            echo "    brightnessctl jq inotify-tools swaybg swayidle mako cliphist qt6ct"
            ;;
    esac

    echo ""
    echo -e "${DIM}After installing, run: ./install.sh${NC}"
    exit 1

elif [[ $WARN -gt 0 ]]; then
    echo -e "${YELLOW}All core dependencies are installed.${NC}"
    echo -e "${DIM}Some optional tools are missing — OCWS will work but features may be limited.${NC}"
    echo ""
    echo -e "${BOLD}Ready to install! Run:${NC} ${GREEN}./install.sh${NC}"
    exit 0

else
    echo -e "${GREEN}All dependencies are installed!${NC}"
    echo ""
    echo -e "${BOLD}Ready to install! Run:${NC} ${GREEN}./install.sh${NC}"
    exit 0
fi
