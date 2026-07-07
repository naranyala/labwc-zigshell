#!/bin/bash
# -------------------------------------------------------------------
# OCWS Installer - Question-Driven Shell Selection
# Guides the user through picking their preferred desktop shell.
# For comprehensive distro-specific installation, use ./install-distribution.sh
# -------------------------------------------------------------------

set -euo pipefail

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "\n${CYAN}==>${NC} $*"; }
pass() { echo -e "  ${GREEN}✓${NC} $*"; }
warn() { echo -e "  ${YELLOW}⚠${NC} $*"; }
fail() { echo -e "  ${RED}✗${NC} $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

info "OCWS Shell Selection"

cat << 'MENU'

  Choose your desktop shell:

    1)  labwc + double-panel sfwbar
        ──────────────────────────────
        Dual-panel layout: top statusbar + bottom dock.
        Full OCWS shell — the standard experience.
        Statusbar + Dock via sfwbar.

    2)  labwc + sfwbar statusbar + crystal-dock
        ────────────────────────────────────────
        Single sfwbar statusbar on top.
        Application dock managed by crystal-dock.
        Classic panel + dock separation.

    3)  labwc + DankMaterialShell
        ────────────────────────────────────────
        Material Design-inspired shell by DankShrine.
        Vertical panel + minimalist layout.

    4)  labwc + Noctalia Shell
        ────────────────────────────────────────
        Quiet-by-design shell from the Noctalia project.
        Minimalist, distraction-free interface.

MENU

echo -n "  Enter choice [1-4] (default: 1): "
read -r mode_choice

case "${mode_choice:-1}" in
    1) MODE="doublepanel"
       MODE_DESC="labwc + double-panel sfwbar"
       ;;
    2) MODE="crystaldock"
       MODE_DESC="labwc + sfwbar statusbar + crystal-dock"
       ;;
    3) MODE="dms"
       MODE_DESC="labwc + DankMaterialShell"
       ;;
    4) MODE="noctalia"
       MODE_DESC="labwc + Noctalia Shell"
       ;;
    *) MODE="doublepanel"
       MODE_DESC="labwc + double-panel sfwbar"
       ;;
esac

echo -e "\n  Selected: ${CYAN}${MODE_DESC}${NC}"

echo -e "\n  Choose your default application launcher:"
echo -e "    1) fuzzel  (minimal, Wayland-native, runs out of the box)"
echo -e "    2) rofi    (feature-rich, customized theme)"
echo -n "  Enter choice [1-2] (default: 1): "
read -r launcher_choice

case "${launcher_choice:-1}" in
    2) LAUNCHER="rofi"
       LAUNCHER_DESC="rofi"
       ;;
    *) LAUNCHER="fuzzel"
       LAUNCHER_DESC="fuzzel"
       ;;
esac

echo -e "  Selected: ${CYAN}${LAUNCHER_DESC}${NC}"

# 1. Dependency Resolution Menu
if [ -f "${SCRIPT_DIR}/install-distribution.sh" ] && [ -f "${SCRIPT_DIR}/build-ocws-core.sh" ]; then
    echo -e "\n  ${GREEN}✓${NC} Installers detected."
    echo -e "  ${CYAN}=== Dependency Resolution ===${NC}"
    echo -e "    1) Auto Install (Use package manager for standard deps)"
    echo -e "    2) Compile Core (Build labwc, sfwbar, fuzzel from source)"
    echo -e "    3) Skip (Just deploy dotfiles)"
    echo -e "\n  Default: 1 (Auto Install)"
    echo -n "    Enter choice [1-3]: "

    read -r choice

    case "${choice:-1}" in
        1)
            echo -e "\n${CYAN}==>${NC} Starting distribution package installer..."
            bash "${SCRIPT_DIR}/install-distribution.sh" "$@"
            ;;
        2)
            echo -e "\n${CYAN}==>${NC} Starting source compilation for core engines..."
            bash "${SCRIPT_DIR}/build-ocws-core.sh" all
            ;;
        *)
            echo -e "\n${CYAN}==>${NC} Skipping standard dependency resolution..."
            ;;
    esac

    # Handle custom/community shells
    if [[ "$MODE" == "dms" || "$MODE" == "crystaldock" ]]; then
        echo -e "\n${YELLOW}⚠${NC} Your chosen shell mode (${MODE}) requires a custom engine."
        echo -n "  Would you like to automatically clone and compile it now? [y/N]: "
        read -r build_community
        if [[ "$build_community" =~ ^[Yy]$ ]]; then
            bash "${SCRIPT_DIR}/build-ocws-core.sh" "$MODE"
        else
            echo -e "  Skipping. You will need to install it manually."
        fi
    elif [[ "$MODE" == "noctalia" ]]; then
        echo -e "\n${YELLOW}⚠${NC} Noctalia is a custom wrapper that requires manual setup."
    fi
fi

# -------------------------------------------------------------------
# Quick Installer
# Manual dependency installation and configuration deployment
# -------------------------------------------------------------------

# 1. Dependency Check
info "Checking for required dependencies..."
MISSING=""
command -v labwc >/dev/null 2>&1 || MISSING="$MISSING labwc"
command -v sfwbar >/dev/null 2>&1 || MISSING="$MISSING sfwbar"
command -v "$LAUNCHER" >/dev/null 2>&1 || MISSING="$MISSING $LAUNCHER"

if [ -n "$MISSING" ]; then
    echo -e "\n${YELLOW}⚠${NC} Missing engines:$MISSING"
    echo -e "  ${RED}Options:${NC}"
    echo -e "    1) Install via package manager (${SCRIPT_DIR}/install-distribution.sh)"
    echo -e "    2) Build from source (${SCRIPT_DIR}/build-ocws-core.sh all)"
    echo -e "\n  Press [ENTER] to continue anyway, or Ctrl+C to cancel."
    read -r
fi

if [[ "$MODE" == "dms" ]] && ! command -v dms >/dev/null 2>&1; then
    echo -e "\n${YELLOW}⚠${NC} Dank Material Shell (dms) is missing!"
    echo -e "  This is a community shell that requires manual installation."
    echo -e "  Recommendation: Compile it from source (e.g., https://github.com/dankshrine/dms)"
    echo -e "\n  Press [ENTER] to continue anyway, or Ctrl+C to cancel."
    read -r
fi

if [[ "$MODE" == "noctalia" ]]; then
    MISSING_NOCTALIA=""
    command -v noctalia >/dev/null 2>&1 || MISSING_NOCTALIA="noctalia "
    command -v sfwbar >/dev/null 2>&1 || MISSING_NOCTALIA+="sfwbar"
    
    if [ -n "$MISSING_NOCTALIA" ]; then
        echo -e "\n${YELLOW}⚠${NC} Missing dependencies for Noctalia: ${MISSING_NOCTALIA}"
        echo -e "  Noctalia is a custom shell that requires manual compilation."
        echo -e "  Recommendation: Build from the official Noctalia repository."
        echo -e "\n  Press [ENTER] to continue anyway, or Ctrl+C to cancel."
        read -r
    fi
fi

if [[ "$MODE" == "crystaldock" ]]; then
    if ! command -v crystal-dock >/dev/null 2>&1; then
        echo -e "\n${YELLOW}⚠${NC} crystal-dock is not installed."
        echo -e "  Recommendation: Clone and build from https://github.com/igrekster/crystal-dock"
        echo -e "\n  Press [ENTER] to continue anyway, or Ctrl+C to cancel."
        read -r
    fi
fi

# 1.5 Confirmation Prompt
echo -e "\n${YELLOW}⚠ WARNING: This will deploy configurations to ~/.config/ and ~/.local/bin/${NC}"
echo -e "  Mode: ${CYAN}${MODE_DESC}${NC}"
echo -e "  Launcher: ${CYAN}${LAUNCHER_DESC}${NC}"
echo -e "  Affected directories: labwc, ocws, foot, gtk-3.0, gtk-4.0, mako, qt6ct"

if [[ "$LAUNCHER" == "rofi" ]]; then
    echo -e "  ${CYAN}  rofi${NC}: ~/.config/rofi/"
fi

case "$MODE" in
    doublepanel)
        echo -e "  Shell: OCWS dual-panel (top bar + dock via sfwbar)"
        echo -e "  ${CYAN}  OCWS:${NC} full bar config, widgets, plugins, themes"
        ;;
    crystaldock)
        echo -e "  Shell: crystal-dock dock + sfwbar single statusbar (${CYAN}requires crystal-dock${NC})"
        echo -e "  ${CYAN}  OCWS:${NC} single top statusbar via sfwbar-full.config"
        ;;
    dms)
        echo -e "  Shell: DankMaterialShell (${CYAN}requires dms${NC})"
        echo -e "  ${CYAN}  OCWS:${NC} infrastructure only (no sfwbar bars)"
        ;;
    noctalia)
        echo -e "  Shell: Noctalia (${CYAN}requires sfwbar${NC})"
        echo -e "  ${CYAN}  OCWS:${NC} infrastructure only (no sfwbar bars)"
        ;;
esac

echo -n "  Are you sure you want to proceed? [y/N]: "
read -r confirm
if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    echo -e "\n  Installation aborted."
    exit 0
fi

# 2. Setup Directories
info "Setting up configuration directories..."
mkdir -p ~/.config/labwc
mkdir -p ~/.config/rofi
mkdir -p ~/.config/ocws/plugins
mkdir -p ~/.config/gtk-3.0 ~/.config/gtk-4.0
mkdir -p ~/.local/bin/actions

case "$MODE" in
    crystaldock) mkdir -p ~/.config/crystal-dock ;;
    dms)         mkdir -p ~/.config/DankMaterialShell ;;
    noctalia)    mkdir -p ~/.config/noctalia ;;
esac
pass "Directories created."

# 3. Deploy Labwc Core
info "Deploying Compositor Rules (labwc)..."
cp -r "$SCRIPT_DIR/dotfiles/labwc/"* ~/.config/labwc/ 2>/dev/null || fail "Failed to deploy labwc configurations"
pass "labwc configurations synced."

# 4. Deploy OCWS Shell (supporting infrastructure for all modes)
info "Deploying the OCWS Shell..."
case "$MODE" in
    doublepanel)
        # Full OCWS dual-panel bar config
        rsync -a --exclude='user.config' "$SCRIPT_DIR/dotfiles/ocws/" ~/.config/ocws/ 2>/dev/null || cp -r "$SCRIPT_DIR/dotfiles/ocws/"* ~/.config/ocws/ 2>/dev/null || fail "Failed to deploy OCWS shell"
        ;;
    crystaldock)
        # OCWS infrastructure + single top statusbar (no dock)
        rsync -a --exclude='user.config' --exclude='ocws.config' "$SCRIPT_DIR/dotfiles/ocws/" ~/.config/ocws/ 2>/dev/null || cp -r "$SCRIPT_DIR/dotfiles/ocws/"* ~/.config/ocws/ 2>/dev/null || fail "Failed to deploy OCWS shell"
        cp "$SCRIPT_DIR/dotfiles/ocws/sfwbar-full.config" ~/.config/ocws/ocws.config 2>/dev/null || true
        ;;
    *)
        # Supporting infrastructure only — no sfwbar bar config at all
        rsync -a --exclude='user.config' --exclude='ocws.config' --exclude='ocws.css' --exclude='theme.css' "$SCRIPT_DIR/dotfiles/ocws/" ~/.config/ocws/ 2>/dev/null || cp -r "$SCRIPT_DIR/dotfiles/ocws/"* ~/.config/ocws/ 2>/dev/null || fail "Failed to deploy OCWS shell"
        ;;
esac

if [ ! -f ~/.config/ocws/user.config ]; then
    cp "$SCRIPT_DIR/dotfiles/ocws/user.config" ~/.config/ocws/user.config 2>/dev/null || true
fi

echo "$MODE" > ~/.config/ocws/mode
echo "$LAUNCHER" > ~/.config/ocws/launcher
pass "OCWS infrastructure, mode ($MODE), and launcher ($LAUNCHER) synced."

# 5. Deploy shell-specific configs
case "$MODE" in
    crystaldock)
        if [ -d "$SCRIPT_DIR/dotfiles/crystal-dock" ]; then
            info "Deploying crystal-dock configuration..."
            rsync -a "$SCRIPT_DIR/dotfiles/crystal-dock/" ~/.config/crystal-dock/ 2>/dev/null || true
            pass "crystal-dock config synced."
        fi
        ;;
    dms)
        if [ -d "$SCRIPT_DIR/dotfiles/DankMaterialShell" ]; then
            info "Deploying Dank Material Shell configuration..."
            rsync -a "$SCRIPT_DIR/dotfiles/DankMaterialShell/" ~/.config/DankMaterialShell/ 2>/dev/null || true
            pass "Dank Material Shell config synced."
        fi
        ;;
    noctalia)
        if [ -d "$SCRIPT_DIR/dotfiles/noctalia" ]; then
            info "Deploying Noctalia configuration..."
            rsync -a "$SCRIPT_DIR/dotfiles/noctalia/" ~/.config/noctalia/ 2>/dev/null || true
            pass "Noctalia config synced."
        fi
        ;;
esac

# 6. Deploy Application Launcher
case "$LAUNCHER" in
    rofi)
        if [ -d "$SCRIPT_DIR/dotfiles/rofi" ]; then
            info "Deploying Application Launcher (rofi)..."
            mkdir -p ~/.config/rofi
            cp -r "$SCRIPT_DIR/dotfiles/rofi/"* ~/.config/rofi/ 2>/dev/null || fail "Failed to deploy rofi configuration"
            pass "Rofi synced."
        fi
        ;;
    fuzzel)
        info "Using fuzzel as application launcher..."
        pass "fuzzel requires no configuration — ready out of the box."
        ;;
esac

# Deploy Foot Terminal
if [ -d "$SCRIPT_DIR/dotfiles/foot" ]; then
    info "Deploying Foot Terminal configuration..."
    mkdir -p ~/.config/foot
    cp -r "$SCRIPT_DIR/dotfiles/foot/"* ~/.config/foot/ 2>/dev/null || true
    pass "Foot synced."
fi

# 7. Deploy GTK Styling
if [ -d "$SCRIPT_DIR/dotfiles/gtk-3.0" ] || [ -d "$SCRIPT_DIR/dotfiles/gtk-4.0" ] || [ -d "$SCRIPT_DIR/dotfiles/gtk" ]; then
    info "Deploying GTK Preferences..."
    mkdir -p ~/.config/gtk-3.0 ~/.config/gtk-4.0
    [ -d "$SCRIPT_DIR/dotfiles/gtk" ] && cp -r "$SCRIPT_DIR/dotfiles/gtk/"* ~/.config/gtk-3.0/ 2>/dev/null || true
    [ -d "$SCRIPT_DIR/dotfiles/gtk" ] && cp -r "$SCRIPT_DIR/dotfiles/gtk/"* ~/.config/gtk-4.0/ 2>/dev/null || true
    [ -d "$SCRIPT_DIR/dotfiles/gtk-3.0" ] && cp -r "$SCRIPT_DIR/dotfiles/gtk-3.0/"* ~/.config/gtk-3.0/ 2>/dev/null || true
    [ -d "$SCRIPT_DIR/dotfiles/gtk-4.0" ] && cp -r "$SCRIPT_DIR/dotfiles/gtk-4.0/"* ~/.config/gtk-4.0/ 2>/dev/null || true
    pass "GTK settings synced."
fi

# Deploy Mako
if [ -d "$SCRIPT_DIR/dotfiles/mako" ]; then
    info "Deploying Mako configuration..."
    mkdir -p ~/.config/mako
    cp -r "$SCRIPT_DIR/dotfiles/mako/"* ~/.config/mako/ 2>/dev/null || true
    pass "Mako synced."
fi

# Deploy qt6ct
if [ -d "$SCRIPT_DIR/dotfiles/qt6ct" ]; then
    info "Deploying Qt6ct configuration..."
    mkdir -p ~/.config/qt6ct
    cp -r "$SCRIPT_DIR/dotfiles/qt6ct/"* ~/.config/qt6ct/ 2>/dev/null || true
    pass "Qt6ct synced."
fi

# Deploy Zebar
if [ -d "$SCRIPT_DIR/dotfiles/.glzr/zebar" ]; then
    info "Deploying Zebar configuration..."
    mkdir -p ~/.glzr/zebar
    cp -r "$SCRIPT_DIR/dotfiles/.glzr/zebar/"* ~/.glzr/zebar/ 2>/dev/null || true
    pass "Zebar synced."
fi

# Deploy extra plugins
if [ -d "$SCRIPT_DIR/dotfiles/zebar" ]; then
    info "Deploying Zebar configuration..."
    mkdir -p ~/.config/zebar
    cp -r "$SCRIPT_DIR/dotfiles/zebar/"* ~/.config/zebar/ 2>/dev/null || true
    pass "Zebar synced."
fi

# 7. Deploy IPC & Core Tools
info "Deploying Event Bus API & System Tools..."
find "$SCRIPT_DIR/scripts" -maxdepth 1 -type f -name "*.sh" -exec cp {} ~/.local/bin/ \; 2>/dev/null || fail "Failed to deploy scripts"
if [ -d "$SCRIPT_DIR/scripts/actions" ]; then
    cp "$SCRIPT_DIR/scripts/actions/"* ~/.local/bin/actions/ 2>/dev/null || true
fi
chmod +x ~/.local/bin/*.sh 2>/dev/null || fail "Failed to set execute permissions on scripts"
chmod +x ~/.local/bin/actions/* 2>/dev/null || true
pass "Scripts and IPC mapped to ~/.local/bin"

# 7b. Deploy dotfiles/wallpaper script
if [ -f "$SCRIPT_DIR/dotfiles/wallpaper" ]; then
    cp "$SCRIPT_DIR/dotfiles/wallpaper" ~/.local/bin/wallpaper
    chmod +x ~/.local/bin/wallpaper
    pass "wallpaper command installed to ~/.local/bin/wallpaper"
fi

# 7c. Deploy wallpaper sources list
if [ -f "$SCRIPT_DIR/dotfiles/wallpaper-sources.txt" ]; then
    cp "$SCRIPT_DIR/dotfiles/wallpaper-sources.txt" ~/.config/ocws/wallpaper-sources.txt
    pass "wallpaper-sources.txt deployed to ~/.config/ocws/"
fi

# 8. Strict Installation Validation
info "Performing strict validation of deployed assets..."

validate_file() {
    local target="$1"
    if [ ! -e "$target" ]; then
        echo -e "  ${RED}✗${NC} Missing deployed asset: $target"
        return 1
    fi
    return 0
}

validate_executable() {
    local target="$1"
    if [ ! -x "$target" ]; then
        echo -e "  ${RED}✗${NC} Missing execute permissions: $target"
        return 1
    fi
    return 0
}

validate_file_format() {
    local target="$1"
    local format="$2"
    
    case "$format" in
        xml)
            if ! command -v xmllint >/dev/null 2>&1; then
                warn "xmllint not available for XML validation of $target"
                return 0
            fi
            if ! xmllint --noout "$target" 2>/dev/null; then
                echo -e "  ${RED}✗${NC} Invalid XML format: $target"
                return 1
            fi
            ;;
        ini)
            if ! command -v crudini >/dev/null 2>&1; then
                warn "crudini not available for INI validation of $target"
                return 0
            fi
            crudini --get "$target" >/dev/null 2>&1 || {
                echo -e "  ${RED}✗${NC} Invalid INI format: $target"
                return 1
            }
            ;;
        css)
            if ! command -v csslint >/dev/null 2>&1; then
                warn "csslint not available for CSS validation of $target"
                return 0
            fi
            csslint "$target" >/dev/null 2>&1 || {
                echo -e "  ${RED}✗${NC} Invalid CSS format: $target"
                return 1
            }
            ;;
        shell)
            bash -n "$target" 2>/dev/null || {
                echo -e "  ${RED}✗${NC} Invalid shell syntax: $target"
                return 1
            }
            ;;
    esac
    pass "Valid $format format: $target"
    return 0
}

validate_content() {
    local target="$1"
    local check_type="$2"
    
    case "$check_type" in
        rcxml)
            if ! grep -q "<labwc_config>" "$target"; then
                echo -e "  ${RED}✗${NC} Missing root element in rc.xml"
                return 1
            fi
            if ! grep -q "<keyboard>" "$target" || ! grep -q "</keyboard>" "$target"; then
                echo -e "  ${RED}✗${NC} Missing keyboard section in rc.xml"
                return 1
            fi
            ;;
        menu)
            if ! grep "<menu" "$target" | grep -q "/>"; then
                echo -e "  ${RED}✗${NC} Missing root menu element in menu.xml"
                return 1
            fi
            ;;
        ocwsconfig)
            if ! grep -q "^bar\|widget" "$target"; then
                echo -e "  ${RED}✗${NC} Missing bar definition in ocws.config"
                return 1
            fi
            ;;
        fuzzelini)
            if ! grep -q "^\[main\]" "$target"; then
                echo -e "  ${RED}✗${NC} Missing [main] section in fuzzel.ini"
                return 1
            fi
            ;;
        scripts)
            # Check for essential shebang
            if [[ "$target" == *.sh ]]; then
                if ! head -1 "$target" | grep -q "^#!/bin/bash"; then
                    echo -e "  ${YELLOW}⚠${NC} Missing bash shebang in $target"
                fi
            fi
            ;;
    esac
    pass "Content validation passed: $target ($check_type)"
    return 0
}

validate_required_functions() {
    local script="$1"
    
    if [[ "$script" == *.sh ]]; then
        # Check for essential functions
        if ! grep -q "^info()" "$script" && ! grep -q "^error()" "$script"; then
            echo -e "  ${YELLOW}⚠${NC} Script $script may lack error handling functions"
        fi
        
        # Check for file existence checks
        if ! grep -q "\[ ! -f\]\|\[-d " "$script"; then
            echo -e "  ${YELLOW}⚠${NC} Script $script may lack file validation"
        fi
    fi
    pass "Required functions present in $script"
    return 0
}

validate_keybinding_integrity() {
    local rc_file="$HOME/.config/labwc/rc.xml"
    
    if [ ! -f "$rc_file" ]; then
        echo -e "  ${YELLOW}⚠${NC} rc.xml not found for keybinding validation"
        return 0
    fi
    
    # Check for duplicate keybindings
    local duplicates=$(sed -n 's/.*key="\([^"]*\)".*/\1/p' "$rc_file" | sort | uniq -d)
    if [ -n "$duplicates" ]; then
        echo -e "  ${RED}✗${NC} Duplicate keybindings found: $duplicates"
        return 1
    fi
    
    # Check for essential keybindings
    local essential_keys=(A-Return A-Alt-Tab A-ESC A-S-ESC A-F4 S-F4)
    for key in "${essential_keys[@]}"; do
        if ! grep -q "key=\"$key\"" "$rc_file"; then
            echo -e "  ${YELLOW}⚠${NC} Missing essential keybinding: $key"
        fi
    done
    
    pass "Keybinding integrity validated"
    return 0
}

ERRORS=0

# Verify Labwc
validate_file "$HOME/.config/labwc/rc.xml" || ((ERRORS++))
validate_file "$HOME/.config/labwc/autostart" || ((ERRORS++))
validate_file "$HOME/.config/labwc/menu.xml" || ((ERRORS++))

# Verify OCWS
if [[ "$MODE" == "doublepanel" || "$MODE" == "crystaldock" ]]; then
    validate_file "$HOME/.config/ocws/ocws.config" || ((ERRORS++))
fi

# Verify Launcher
if [[ "$LAUNCHER" == "rofi" ]]; then
    validate_file "$HOME/.config/rofi/config.rasi" || ((ERRORS++))
fi

# Verify shell-specific configs
case "$MODE" in
    crystaldock)
        validate_file "$HOME/.config/crystal-dock" || ((ERRORS++)) ;;
    dms)
        validate_file "$HOME/.config/DankMaterialShell" || ((ERRORS++)) ;;
    noctalia)
        validate_file "$HOME/.config/noctalia" || ((ERRORS++)) ;;
esac

# Verify Scripts
for script in "$SCRIPT_DIR/scripts/"*.sh; do
    [ -e "$script" ] || continue
    base=$(basename "$script")
    validate_file "$HOME/.local/bin/$base" || ((ERRORS++))
    validate_executable "$HOME/.local/bin/$base" || ((ERRORS++))
    validate_required_functions "$HOME/.local/bin/$base"
    validate_file_format "$HOME/.local/bin/$base" shell
    validate_content "$HOME/.local/bin/$base" scripts
    done

# Verify Dotfile Formats
validate_file_format "$HOME/.config/labwc/rc.xml" xml
validate_file_format "$HOME/.config/labwc/menu.xml" xml

if [[ "$MODE" == "doublepanel" || "$MODE" == "crystaldock" ]] && [ -f "$HOME/.config/ocws/ocws.config" ]; then
    validate_content "$HOME/.config/ocws/ocws.config" ocwsconfig
fi

if [[ "$LAUNCHER" == "rofi" ]] && [ -f "$HOME/.config/rofi/config.rasi" ]; then
    validate_file_format "$HOME/.config/rofi/config.rasi" rasi
    pass "Rofi format validated"
fi

# Validate Labwc content
validate_content "$HOME/.config/labwc/rc.xml" rcxml
validate_content "$HOME/.config/labwc/menu.xml" menu

# Validate keybinding integrity
validate_keybinding_integrity

if [ "$ERRORS" -gt 0 ]; then
    fail "Comprehensive validation failed with $ERRORS errors. Installation is incomplete."
else
    pass "All configuration files strictly validated with format, content, and integrity checks."
fi

# 9. Success
info "OCWS Deployment Complete! 🚀"
echo -e "\n${CYAN}=== Quick Install Complete ===${NC}"
echo -e "  Installed Mode: ${GREEN}${MODE_DESC}${NC}"
echo -e "  Launcher: ${GREEN}${LAUNCHER_DESC}${NC}"
echo -e "${CYAN}  Note:${NC} You must manually install labwc, sfwbar, and ${LAUNCHER} first."
echo -e "  Use ./install-distribution.sh for automatic distro detection and installation."
echo -e "\n${CYAN}  Next Steps:${NC}"
echo -e "  • Install dependencies using: ./install-distribution.sh (Recommended)"
echo -e "  • Build from source: ./build-ocws-core.sh all"
echo -e "  • Restart and select 'labwc' from display manager"
echo -e "  • Or run: labwc (from a TTY)"
