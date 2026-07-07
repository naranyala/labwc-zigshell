#!/bin/bash
# -------------------------------------------------------------------
# OCWS Comprehensive Distribution Installer
# -------------------------------------------------------------------

set -euo pipefail

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "\n${CYAN}==>${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

info "Detecting Linux Distribution..."

if [ ! -f /etc/os-release ]; then
    fail "Cannot detect OS (/etc/os-release missing). Please use manual quick install."
fi

. /etc/os-release
OS=$ID
OS_LIKE=${ID_LIKE:-$ID}

DISTRO_SCRIPT=""

case "$OS" in
    arch|manjaro|endeavouros)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/arch.sh"
        ;;
    debian|ubuntu|pop|linuxmint)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/debian.sh"
        ;;
    fedora)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/fedora.sh"
        ;;
    opensuse*|suse)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/suse.sh"
        ;;
    alpine)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/alpine.sh"
        ;;
    void)
        DISTRO_SCRIPT="$SCRIPT_DIR/distro/void.sh"
        ;;
    *)
        # Fallback to ID_LIKE checks
        if echo "$OS_LIKE" | grep -q "arch"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/arch.sh"
        elif echo "$OS_LIKE" | grep -q "debian"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/debian.sh"
        elif echo "$OS_LIKE" | grep -q "fedora"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/fedora.sh"
        elif echo "$OS_LIKE" | grep -q "suse"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/suse.sh"
        elif echo "$OS_LIKE" | grep -q "alpine"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/alpine.sh"
        elif echo "$OS_LIKE" | grep -q "void"; then
            DISTRO_SCRIPT="$SCRIPT_DIR/distro/void.sh"
        else
            fail "Unsupported distribution: $PRETTY_NAME. Please use quick install."
        fi
        ;;
esac

echo -e "  ${GREEN}✓${NC} Detected: $PRETTY_NAME"

if [ ! -f "$DISTRO_SCRIPT" ]; then
    fail "Distribution script not found at $DISTRO_SCRIPT"
fi

info "Executing distribution-specific installer..."
bash "$DISTRO_SCRIPT"

info "Post-install configuration sync complete."
