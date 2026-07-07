#!/bin/bash
set -euo pipefail

echo "Installing OCWS dependencies for Void Linux..."

PKGS=(
    labwc sfwbar rofi-wayland foot mako qt6ct fuzzel bc
    swaybg swayidle swaylock gammastep dunst
    playerctl wl-clipboard cliphist grim slurp flameshot
    jq crudini libxml2 brightnessctl wlr-randr nautilus
    gnome-keyring xdotool inotify-tools ImageMagick wireplumber
)

# Install packages
sudo xbps-install -Su
sudo xbps-install -Sy "${PKGS[@]}"

# Warn about community shells
if ! command -v crystal-dock >/dev/null 2>&1; then
    echo -e "\nNote: crystal-dock must be compiled from source for Void Linux:"
    echo "  https://github.com/igrekster/crystal-dock"
fi

if ! command -v dms >/dev/null 2>&1; then
    echo -e "\nNote: dms (DankMaterialShell) must be compiled from source for Void Linux:"
    echo "  https://github.com/DankShrine/dms"
fi

echo -e "\nDependencies successfully installed."
