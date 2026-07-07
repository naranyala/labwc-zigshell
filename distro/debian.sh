#!/bin/bash
set -euo pipefail

echo "Installing OCWS dependencies for Debian/Ubuntu..."

sudo apt update

PKGS=(
    labwc rofi-wayland foot mako-notifier qt6ct
    swaybg swayidle swaylock gammastep
    playerctl wl-clipboard grim slurp flameshot
    jq crudini libxml2-utils brightnessctl wlr-randr
    nautilus gnome-keyring xdotool inotify-tools imagemagick wireplumber
    network-manager bluez libnotify-bin fonts-noto fonts-dejavu-core
    rsync
)

# sfwbar — not always in default repos
if apt-cache show sfwbar >/dev/null 2>&1; then
    PKGS+=(sfwbar)
else
    echo ""
    echo "  sfwbar not found in apt repos. Build from source:"
    echo "    https://github.com/sfwbar/sfwbar"
fi

# cliphist — often missing in older Ubuntu/LTS
if apt-cache show cliphist >/dev/null 2>&1; then
    PKGS+=(cliphist)
else
    echo ""
    echo "  cliphist not found. Clipboard history will use wl-paste fallback."
fi

# fuzzel — may not be in stable repos
if apt-cache show fuzzel >/dev/null 2>&1; then
    PKGS+=(fuzzel)
else
    echo ""
    echo "  fuzzel not found. Use rofi or build from source:"
    echo "    https://codeberg.org/dnkl/fuzzel"
fi

# crystal-dock
if apt-cache show crystal-dock >/dev/null 2>&1; then
    PKGS+=(crystal-dock)
else
    echo ""
    echo "  crystal-dock not found. Build from source:"
    echo "    https://github.com/crystal-dock/crystal-dock"
fi

# FiraCode Nerd Font — primary icon font
PKGS+=(fonts-firacode)
if ! apt-cache show fonts-firacode >/dev/null 2>&1; then
    echo ""
    echo "  FiraCode fonts not in repos. Download manually:"
    echo "    https://github.com/ryanoasis/nerd-fonts/releases"
fi

sudo apt install -y "${PKGS[@]}"

# dms (DankMaterialShell) — needs manual build
if ! command -v dms >/dev/null 2>&1; then
    echo ""
    echo "  For DMS mode — build from source:"
    echo "    sudo apt install gcc make pkg-config libgtk-3-dev libjson-c-dev"
    echo "    git clone https://github.com/DankShrine/dms.git"
    echo "    cd dms && make && sudo make install"
fi

echo ""
echo "Dependencies successfully installed."
