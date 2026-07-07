#!/bin/bash
set -euo pipefail

echo "Installing OCWS dependencies for openSUSE..."

PKGS=(
    labwc sfwbar rofi-wayland foot mako qt6ct
    swaybg swayidle swaylock gammastep
    playerctl wl-clipboard cliphist grim slurp flameshot
    jq crudini libxml2-tools brightnessctl wlr-randr
    nautilus gnome-keyring xdotool inotify-tools ImageMagick wireplumber
    fuzzel NetworkManager bluez libnotify-tools
    google-noto-sans-fonts google-noto-sans-mono-fonts dejavu-fonts
    rsync
)

# FiraCode Nerd Font
if zypper search fira-code-fonts 2>/dev/null | grep -qi "fira-code"; then
    PKGS+=(fira-code-fonts)
else
    echo ""
    echo "  FiraCode fonts not found. Download manually:"
    echo "    https://github.com/ryanoasis/nerd-fonts/releases"
fi

# crystal-dock — not in openSUSE repos
if ! command -v crystal-dock >/dev/null 2>&1; then
    echo ""
    echo "  For crystal-dock mode — build from source:"
    echo "    sudo zypper install gcc make pkg-config gtk3-devel"
    echo "    git clone https://github.com/crystal-dock/crystal-dock.git"
    echo "    cd crystal-dock && make && sudo make install"
fi

sudo zypper install -y "${PKGS[@]}"

# dms (DankMaterialShell) — needs manual build
if ! command -v dms >/dev/null 2>&1; then
    echo ""
    echo "  For DMS mode — build from source:"
    echo "    sudo zypper install gcc make pkg-config gtk3-devel libjson-c-devel"
    echo "    git clone https://github.com/DankShrine/dms.git"
    echo "    cd dms && make && sudo make install"
fi

echo ""
echo "Dependencies successfully installed."
