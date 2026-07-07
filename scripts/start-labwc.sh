#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

pass()  { echo -e "  ${GREEN}✓${NC} $1"; }
fail()  { echo -e "  ${RED}✗${NC} $1"; }
warn()  { echo -e "  ${YELLOW}⚠${NC} $1"; }
info()  { echo -e "  ${CYAN}→${NC} $1"; }

echo ""
echo "== labwc Launch Check =="
echo ""

# --- 1. Binary check ---
LABWC_BIN=""
for p in "$HOME/.local/bin/labwc" "/usr/local/bin/labwc" "/usr/bin/labwc"; do
  if [ -x "$p" ]; then LABWC_BIN="$p"; break; fi
done
if [ -z "$LABWC_BIN" ] && command -v labwc &>/dev/null; then
  LABWC_BIN="$(command -v labwc)"
fi

if [ -n "$LABWC_BIN" ]; then
  pass "labwc binary found: $LABWC_BIN"
else
  fail "labwc binary not found in PATH"
  info "Build from source: ./download-labwc.sh"
  info "Or install via package manager"
  exit 1
fi

# --- 2. Config check ---
CONFIG_DIR=""
for cfg_dir in "$HOME/.config/labwc" "$PROJECT_DIR/dotfiles/labwc" "$PROJECT_DIR/config/labwc"; do
  if [ -d "$cfg_dir" ]; then
    CONFIG_DIR="$cfg_dir"
    break
  fi
done

if [ -z "$CONFIG_DIR" ]; then
  fail "No labwc config directory found"
  info "Run: ./dotfiles/install.sh"
  info "Or create config manually: mkdir -p ~/.config/labwc"
  exit 1
fi
pass "Config dir found: $CONFIG_DIR"

# --- 3. Dependency check ---
info "Checking dependencies..."
DEPS=(swaybg)
LEGACY_OPTIONAL_DEPS=(crystal-dock noctalia)
MISSING=()
for dep in "${DEPS[@]}"; do
  if ! command -v "$dep" &>/dev/null; then
    MISSING+=("$dep")
  fi
done
if [ ${#MISSING[@]} -gt 0 ]; then
  fail "Missing required dependencies: ${MISSING[*]}"
  info "Install swaybg and try again"
  exit 1
fi
pass "Required dependencies OK"

# Check for legacy dependencies with migration suggestions
LEGACY_FOUND=()
for dep in "${LEGACY_OPTIONAL_DEPS[@]}"; do
  if command -v "$dep" &>/dev/null; then
    LEGACY_FOUND+=("$dep")
  fi
done

if [ ${#LEGACY_FOUND[@]} -gt 0 ]; then
  warn "Legacy dependencies found (should be phased out for OCWS-only setup): ${LEGACY_FOUND[*]}"
  info "Next OCWS release will make these optional. Use sfwbar-plus mode for enhanced OCWS features."
fi

NEW_OPT_MISSING=()
for dep in "${NEW_OPTIONAL_DEPS[@]}"; do
  if ! command -v "$dep" &>/dev/null; then
    NEW_OPT_MISSING+=("$dep")
  fi
done
if [ ${#NEW_OPT_MISSING[@]} -gt 0 ]; then
  warn "OCWS dependencies not found: ${NEW_OPT_MISSING[*]}"
  info "Some OCWS features may not work without them"
fi

# --- 4. Wayland session check ---
echo ""
echo "== Launch Context =="
echo ""
if [ -n "${WAYLAND_DISPLAY:-}" ]; then
  warn "Detected running inside a Wayland session (WAYLAND_DISPLAY=$WAYLAND_DISPLAY)"
  info "You cannot start a compositor from inside another compositor."
  info ""
  info "To launch labwc:"
  info "  1. Switch to a TTY (Ctrl+Alt+F2 or F3)"
  info "  2. Log in"
  info "  3. Run: exec $LABWC_BIN"
  info ""
  info "Or install a display-manager session file:"
  info "  sudo tee /usr/share/wayland-sessions/labwc.desktop << 'EOF'"
  info "  [Desktop Entry]"
  info "  Name=labwc"
  info "  Comment=Lab Wayland Compositor"
  info "  Exec=$LABWC_BIN"
  info "  Type=Application"
  info "  EOF"
  info ""
  info "Then log out and select labwc from the login screen."
  exit 0
fi

# --- 5. Launch ---
pass "Ready to launch labwc"
echo ""
echo "Starting labwc..."
echo ""
exec "$LABWC_BIN"
