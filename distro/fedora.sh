#!/bin/bash
set -euo pipefail

echo "Installing OCWS dependencies for Fedora..."

PKGS=(
    labwc rofi-wayland foot mako qt6ct
    swaybg swayidle swaylock gammastep
    playerctl wl-clipboard cliphist grim slurp flameshot
    jq crudini libxml2 brightnessctl wlr-randr
    nautilus gnome-keyring xdotool inotify-tools ImageMagick wireplumber
    fuzzel NetworkManager bluez libnotify
    google-noto-sans-fonts google-noto-sans-mono-fonts dejavu-sans-fonts
    rsync
)

if dnf search sfwbar | grep -qi "sfwbar"; then
    PKGS+=(sfwbar)
else
    echo ""
    echo "  sfwbar not found in DNF repos. Try COPR or build from source:"
    echo "    sudo dnf copr enable erikreider/SwayTools"
    echo "    sudo dnf install sfwbar"
    echo "    or: https://github.com/sfwbar/sfwbar"
fi

# FiraCode Nerd Font — primary icon font
if dnf search fira-code-nerd-fonts | grep -qi "fira-code-nerd"; then
    PKGS+=(fira-code-nerd-fonts)
elif dnf search fira-code-fonts | grep -qi "fira-code"; then
    PKGS+=(fira-code-fonts)
else
    echo ""
    echo "  FiraCode fonts not in DNF repos. Download manually:"
    echo "    https://github.com/ryanoasis/nerd-fonts/releases"
fi

# crystal-dock — not in Fedora repos
if ! command -v crystal-dock >/dev/null 2>&1; then
    echo ""
    echo "  For crystal-dock mode — build from source:"
    echo "    sudo dnf install gcc make pkg-config gtk3-devel"
    echo "    git clone https://github.com/crystal-dock/crystal-dock.git"
    echo "    cd crystal-dock && make && sudo make install"
fi

sudo dnf install -y "${PKGS[@]}"

# dms (DankMaterialShell) — needs manual build
if ! command -v dms >/dev/null 2>&1; then
    echo ""
    echo "  For DMS mode — build from source:"
    echo "    sudo dnf install gcc make pkg-config gtk3-devel json-c-devel"
    echo "    git clone https://github.com/DankShrine/dms.git"
    echo "    cd dms && make && sudo make install"
fi

echo "Dependencies successfully installed."
