#!/bin/bash
# -------------------------------------------------------------------
# OCWS Universal Dependency Installer
# Detects the current Linux Distribution and routes to the correct
# package installation script.
# -------------------------------------------------------------------

set -euo pipefail

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISTRO_DIR="$SCRIPT_DIR/distro"

info() { echo -e "\n${CYAN}==>${NC} $*"; }
fail() { echo -e "\n${RED}✗${NC} $*"; exit 1; }

# Parse OS Release
if [ -f /etc/os-release ]; then
    . /etc/os-release
else
    fail "/etc/os-release not found. Cannot detect distribution."
fi

info "Detected Distribution: $NAME"

# Map ID or ID_LIKE to our supported distributions
DISTRO_FAMILY=""

if [[ "$ID" == "arch" || "${ID_LIKE:-}" == *"arch"* ]]; then
    DISTRO_FAMILY="arch"
elif [[ "$ID" == "debian" || "$ID" == "ubuntu" || "${ID_LIKE:-}" == *"debian"* || "${ID_LIKE:-}" == *"ubuntu"* ]]; then
    DISTRO_FAMILY="debian"
elif [[ "$ID" == "fedora" || "${ID_LIKE:-}" == *"fedora"* ]]; then
    DISTRO_FAMILY="fedora"
elif [[ "$ID" == "opensuse"* || "$ID" == "suse" || "${ID_LIKE:-}" == *"suse"* ]]; then
    DISTRO_FAMILY="suse"
elif [[ "$ID" == "alpine" ]]; then
    DISTRO_FAMILY="alpine"
elif [[ "$ID" == "void" ]]; then
    DISTRO_FAMILY="void"
else
    fail "Unsupported distribution: $ID ($NAME). Please install dependencies manually."
fi

info "Routing to $DISTRO_FAMILY package installer..."

TARGET_SCRIPT="$DISTRO_DIR/$DISTRO_FAMILY.sh"

if [ -f "$TARGET_SCRIPT" ]; then
    chmod +x "$TARGET_SCRIPT"
    bash "$TARGET_SCRIPT"
else
    fail "Distro script $TARGET_SCRIPT not found!"
fi

echo -e "\n${GREEN}✓ All base dependencies installed!${NC}"
echo "You can now run: ./build-ocws-core.sh all"
