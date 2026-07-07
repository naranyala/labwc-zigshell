#!/bin/bash
set -euo pipefail

echo "Installing OCWS dependencies for Arch Linux..."

PKGS=(
    labwc sfwbar rofi-wayland foot mako qt6ct
    swaybg swayidle swaylock gammastep
    playerctl wl-clipboard cliphist grim slurp flameshot
    jq crudini libxml2 brightnessctl wlr-randr
    nautilus gnome-keyring xdotool inotify-tools imagemagick wireplumber
    fuzzel networkmanager bluez libnotify noto-fonts ttf-dejavu
    rsync
)

# Use paru or yay if available, otherwise fallback to pacman
if command -v paru >/dev/null 2>&1; then
    paru -S --needed "${PKGS[@]}"
elif command -v yay >/dev/null 2>&1; then
    yay -S --needed "${PKGS[@]}"
else
    sudo pacman -S --needed "${PKGS[@]}"
fi

# FiraCode Nerd Font — primary icon font, may need AUR
if ! fc-list | grep -qi "FiraCode Nerd" 2>/dev/null; then
    echo ""
    echo "  Recommended: Install ttf-firacode-nerd (AUR) for icon support"
    echo "    paru -S ttf-firacode-nerd"
    echo "    yay -S ttf-firacode-nerd"
fi

# crystal-dock — not in official repos, needs AUR
if ! command -v crystal-dock >/dev/null 2>&1; then
    echo ""
    echo "  For crystal-dock mode:"
    echo "    paru -S crystal-dock"
    echo "    yay -S crystal-dock"
fi

# dms (DankMaterialShell) — needs to be built from source
if ! command -v dms >/dev/null 2>&1; then
    echo ""
    echo "  For DMS mode — build from source:"
    echo "    git clone https://github.com/DankShrine/dms.git"
    echo "    cd dms && make && sudo make install"
fi

echo "Dependencies successfully installed."
