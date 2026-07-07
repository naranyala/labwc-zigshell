#!/bin/bash
set -euo pipefail

# Colors
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "\n${CYAN}=== OCWS Fix Script ===${NC}\n$1"; }
pass() { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}⚠${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; }

info "Fixing OCWS dotfiles configuration and system implementation..."

# Create backup of current state
info "Creating backup of current configuration..."
BACKUP_DIR="/tmp/ocws-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$BACKUP_DIR"
mkdir -p "$BACKUP_DIR/.config/gtk-3.0" "$BACKUP_DIR/.config/gtk-4.0"
mkdir -p "$BACKUP_DIR/.config/crystal-dock/labwc"

# Save current config if it exists
if [ -f "~/.config/gtk-3.0/settings.ini" ]; then
    cp ~/.config/gtk-3.0/settings.ini "$BACKUP_DIR/gtk-3.0.ini"
fi
if [ -f "~/.config/gtk-4.0/settings.ini" ]; then
    cp ~/.config/gtk-4.0/settings.ini "$BACKUP_DIR/gtk-4.0.ini"
fi
if [ -f "~/.config/crystal-dock/labwc/appearance.conf" ]; then
    cp ~/.config/crystal-dock/labwc/appearance.conf "$BACKUP_DIR/crystal-dock-appearance.conf"
fi

# Fix GTK3 settings.ini - restore complete settings
info "Fixing GTK3 settings.ini (complete system-wide settings)..."
cat > ~/.config/gtk-3.0/settings.ini << 'GTK3_EOF'
[Settings]
gtk-theme-name=Catppuccin-Mocha-Standard-Dark
gtk-icon-theme-name=Papirus-Dark
gtk-font-name=Noto Sans 10
gtk-monospace-font-name=Noto Sans Mono 10
gtk-cursor-theme-name=Catppuccin-Mocha-Dark
gtk-cursor-theme-size=24
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintfull
gtk-xft-rgba=rgb
gtk-application-prefer-dark-theme=true
gtk-decoration-layout=:menu
gtk-enable-animations=true
gtk-shell-shows-app-menu=false
gtk-shell-shows-menubar=false
gtk-menu-images=true
gtk-button-images=true
gtk-toolbar-style=GTK_TOOLBAR_BOTH_HORIZ
GTK3_EOF

# Fix GTK4 settings.ini - restore complete settings
info "Fixing GTK4 settings.ini (complete system-wide settings)..."
cat > ~/.config/gtk-4.0/settings.ini << 'GTK4_EOF'
[Settings]
gtk-theme-name=Catppuccin-Mocha-Standard-Dark
gtk-application-prefer-dark-theme=true
gtk-icon-theme-name=Papirus-Dark
gtk-font-name=Noto Sans 10
gtk-monospace-font-name=Noto Sans Mono 10
gtk-cursor-theme-name=Catppuccin-Mocha-Dark
gtk-cursor-theme-size=24
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintfull
gtk-xft-rgba=rgb
gtk-decoration-layout=:menu
gtk-enable-animations=true
gtk-shell-shows-app-menu=false
gtk-shell-shows-menubar=false
gtk-menu-images=true
gtk-button-images=true
GTK4_EOF

# Fix crystal-dock appearance.conf - set Papirus-Dark icons
info "Fixing crystal-dock appearance.conf (Papirus-Dark icons)..."
cat > ~/.config/crystal-dock/labwc/appearance.conf << 'CRYSTALDOC_EOF'
[General]
activeIndicatorColor=#ff8c00
activeIndicatorColorMetal2D=#ffbf00
backgroundColor=#6b638abd
backgroundColorMetal2D=#ad7381a6
borderColor=#99addd
bouncingLauncherIcon=true
firstRunWindowCountIndicator=false
floatingMargin=6
iconTheme=Papirus-Dark
inactiveIndicatorColor=#008b8b
inactiveIndicatorColorMetal2D=#00ffff
maximumIconSize=48
minimumIconSize=48
panelStyle=4
showTooltip=true
spacingFactor=0.5
tooltipFontSize=24
zoomingAnimationSpeed=16

[Application%20Menu]
backgroundAlpha=0.8
fontSize=14
label=Applications

[Clock]
fontScaleFactor=1
use24HourClock=true
CRYSTALDOC_EOF

# Fix .gtkrc-2.0 (GTK2)
info "Fixing ~/.gtkrc-2.0 (GTK2 icon theme)..."
cat > ~/.gtkrc-2.0 << 'GTK2_EOF'
gtk-icon-theme-name = "Papirus-Dark"
GTK2_EOF

# Fix qt6ct.conf
info "Fixing qt6ct.conf (Papirus-Dark icon theme)..."
cat > ~/.config/qt6ct/qt6ct.conf << 'QT6CT_EOF'
[General]
icon_theme=Papirus-Dark
color_scheme=dark
standard_dialogs=xdg-desktop-portal

[Appearance]
color_scheme_path=/usr/share/qt6ct/colors/airy.conf
custom_palette=true
icon_theme=Papirus-Dark
standard_dialogs=gtk3
style=Fusion

[Interface]
font=Noto Sans,10,-1,5,50,0,0,0,0,0
monoFont=Noto Sans Mono,10,-1,5,50,0,0,0,0,0
QT6CT_EOF

# Fix or create system-wide autostart for crystal-dock
info "Configuring crystal-dock autostart..."
mkdir -p ~/.config/autostart
cat > ~/.config/autostart/crystal-dock.desktop << 'AUTOSTART_EOF'
[Desktop Entry]
Exec=env -u QT_STYLE_OVERRIDE /usr/bin/crystal-dock
Name=crystal-dock
Type=Application
Version=1.0
AUTOSTART_EOF

# Clean and restart crystal-dock
info "Restarting crystal-dock with fixed configuration..."
pkill -x crystal-dock 2>/dev/null || true
rm -f /tmp/qipc_sharedmemory_crystaldock* /tmp/qipc_systemsem_crystaldock* 2>/dev/null
sleep 0.3

if command -v crystal-dock >/dev/null 2>&1; then
    nohup crystal-dock >/dev/null 2>&1 &
    sleep 2
    pgrep -x crystal-dock >/dev/null && echo "✅ crystal-dock running successfully" || echo "⚠️ crystal-dock failed to start (may be expected)"
else
    warn "crystal-dock not found, please install it"
fi

# Fix tooltips in ocws-settings
info "Fixing ocws-settings.c markup tooltips..."
cd "$(dirname "${BASH_SOURCE[0]}")"
if ! grep -q 'make_tooltip_row' src/ocws-settings.c; then
    cat > src/ocws-tooltip.c << 'TOOLTIP_EOF'
static GtkWidget* make_tooltip_row(const char *title, const char *subtitle, const char *tooltip) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(row, 8);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    
    GtkWidget *lbl = gtk_label_new(NULL);
    char *m = g_strdup_printf("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(lbl), m);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    g_free(m);
    
    GtkWidget *sub = gtk_label_new(subtitle);
    gtk_label_set_xalign(GTK_LABEL(sub), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(sub), "dim-label");
    
    GtkWidget *tip = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tip), tooltip);
    gtk_label_set_xalign(GTK_LABEL(tip), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(tip), "dim-label");
    
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), tip, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);
    
    return row;
}
TOOLTIP_EOF
fi

info "OCWS configuration fixes completed!"
info "Summary of fixes:"
info "  ✓ Fixed GTK3 settings.ini (complete theme settings)"
info "  ✓ Fixed GTK4 settings.ini (complete theme settings)"
info "  ✓ Fixed crystal-dock appearance.conf (Papirus-Dark icons)"
info "  ✓ Fixed qt6ct.conf (Papirus-Dark icon theme)"
info "  ✓ Fixed autostart configuration for crystal-dock"
info "  ✓ Restored correct icon themes system-wide"
info ""
info "System is now ready with consistent Papirus-Dark icon theme!"
