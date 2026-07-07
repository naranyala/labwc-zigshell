#!/bin/bash
set -euo pipefail

# ocws-deps.sh — Check OCWS dependencies
# Verifies all required tools and libraries are installed.

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
WARN=0
FAIL=0

pass() { echo -e "  ${GREEN}PASS${NC} $1"; PASS=$((PASS+1)); }
warn() { echo -e "  ${YELLOW}WARN${NC} $1"; WARN=$((WARN+1)); }
fail() { echo -e "  ${RED}FAIL${NC} $1"; FAIL=$((FAIL+1)); }

echo "=== OCWS Dependency Check ==="
echo ""

# --- 1. Core Engines ---
echo "[1/5] Core Engines"
for cmd in labwc sfwbar rofi; do
    if command -v "$cmd" &>/dev/null; then
        pass "$cmd: $(which $cmd)"
    else
        fail "$cmd not found"
    fi
done
echo ""

# --- 2. Terminal ---
echo "[2/5] Terminal"
if command -v foot &>/dev/null; then
    pass "foot: $(which foot)"
else
    fail "foot not found"
fi
echo ""

# --- 3. Build Tools ---
echo "[3/5] Build Tools"
for cmd in git meson ninja pkg-config gcc zig; do
    if command -v "$cmd" &>/dev/null; then
        pass "$cmd"
    else
        warn "$cmd not found (needed for building)"
    fi
done
echo ""

# --- 4. Runtime Dependencies ---
echo "[4/5] Runtime Dependencies"
for cmd in playerctl wl-copy wl-paste grim slurp jq brightnessctl cliphist inotifywait swaybg swayidle swaylock mako dunst rofi qt6ct bc tesseract rsync; do
    if command -v "$cmd" &>/dev/null; then
        pass "$cmd"
    else
        warn "$cmd not found (optional)"
    fi
done
echo ""

# --- 5. C Libraries ---
echo "[5/5] C Libraries (for building OCWS utilities)"
for lib in gtk+-3.0 glib-2.0 gtk-layer-shell-0 cairo wayland-client libpulse lept; do
    if pkg-config --exists "$lib" 2>/dev/null; then
        pass "$lib"
    else
        warn "$lib dev package not found"
    fi
done
echo ""

# --- Summary ---
echo "=== Dependency Check Complete ==="
echo -e "  ${GREEN}PASS: $PASS${NC}"
echo -e "  ${YELLOW}WARN: $WARN${NC}"
echo -e "  ${RED}FAIL: $FAIL${NC}"
echo ""

if [[ "$FAIL" -gt 0 ]]; then
    echo -e "${RED}Some critical dependencies are missing.${NC}"
    echo "Run: ./install-dependencies.sh"
    exit 1
elif [[ "$WARN" -gt 0 ]]; then
    echo -e "${YELLOW}Some optional dependencies are missing. OCWS will work but some features may be limited.${NC}"
    exit 0
else
    echo -e "${GREEN}All dependencies are installed!${NC}"
    exit 0
fi
