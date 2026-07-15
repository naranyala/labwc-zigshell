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
DIM='\033[2m'
NC='\033[0m'

info() { echo -e "\n${CYAN}==>${NC} $*"; }
pass() { echo -e "  ${GREEN}✓${NC} $*"; }
warn() { echo -e "  ${YELLOW}⚠${NC} $*"; }
fail() { ocws_notify_error "OCWS Install" "$*"; echo -e "  ${RED}✗${NC} $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Centralized error handling + desktop notifications (ocws-notify / mako / dunst)
source "$SCRIPT_DIR/scripts/lib/ocws-err.sh"
ocws_enable_strict

check_requirements() {
    # Run the detailed requirements checker if available
    if [ -f "$SCRIPT_DIR/scripts/ocws-check-requirements.sh" ]; then
        bash "$SCRIPT_DIR/scripts/ocws-check-requirements.sh" 2>/dev/null
        local status=$?
        if [ $status -ne 0 ]; then
            echo ""
            echo -e "${YELLOW}Install missing dependencies, then run:${NC} ${GREEN}./install.sh${NC}"
            exit 1
        fi
    else
        # Fallback: minimal check
        info "Running Pre-Flight Check..."

        local missing=()
        if ! command -v git >/dev/null 2>&1; then missing+=("git"); fi
        if ! command -v labwc >/dev/null 2>&1; then missing+=("labwc"); fi
        if ! command -v fuzzel >/dev/null 2>&1; then missing+=("fuzzel"); fi

        if [ ${#missing[@]} -ne 0 ]; then
            echo -e "\n${RED}✗ Missing required dependencies:${NC}"
            for dep in "${missing[@]}"; do
                echo -e "    - ${YELLOW}$dep${NC}"
            done
            echo -e "\n  ${CYAN}Run: ./scripts/ocws-check-requirements.sh${NC}"
            echo -e "  ${CYAN}Or install manually and run ./install.sh again.${NC}"
            exit 1
        fi

        if ! command -v zig >/dev/null 2>&1; then
            echo -e "\n${YELLOW}⚠ zig not found — C binaries will use pre-built versions${NC}"
        fi

        pass "Core requirements met!"
    fi
}

# Run requirements check before proceeding
check_requirements

info "OCWS Shell Selection"

echo -e "\n  ${CYAN}Shell:${NC}"
echo -e "    1)  OCWS Double Panel     7)  LXQt Classic"
echo -e "    2)  DankMaterialShell      8)  LXQt Minimal"
echo -e "    3)  Noctalia Shell        9)  LXQt Standalone"
echo -e "    4)  OCWS Minimal          10)  LXQt Dual Panels"
echo -e "    5)  LXQt Tworow           11)  LXQt Vertical"
echo -e "    6)  LXQt Bottom           12)  zigshell-cairo-pango (merged panel + dock)"
echo -e "                               13)  zigshell-blend2d (merged panel + dock, Blend2D)"

echo -n "  Choice [1-13] (default: 13): "
if [ "${NONINTERACTIVE:-0}" = "1" ]; then
    echo "13 (auto)"
    mode_choice="13"
else
    read -r mode_choice
fi
case "${mode_choice:-13}" in
    1)  MODE="doublepanel";      MODE_DESC="OCWS Double Panel" ;;
    2)  MODE="dms";              MODE_DESC="DankMaterialShell" ;;
    3)  MODE="noctalia";         MODE_DESC="Noctalia Shell" ;;
    4)  MODE="minimal";          MODE_DESC="OCWS Minimal" ;;
    5)  MODE="tworow";           MODE_DESC="LXQt Tworow" ;;
    6)  MODE="lxqt-bottom";      MODE_DESC="LXQt Bottom" ;;
    7)  MODE="lxqt-classic";     MODE_DESC="LXQt Classic" ;;
    8)  MODE="lxqt-minimal";     MODE_DESC="LXQt Minimal" ;;
    9)  MODE="lxqt-standalone";  MODE_DESC="LXQt Standalone" ;;
    10) MODE="lxqt-dual-lxqt";   MODE_DESC="LXQt Dual Panels" ;;
    11) MODE="lxqt-vertical";    MODE_DESC="LXQt Vertical" ;;
    12) MODE="zigshell-cairo-pango";   MODE_DESC="zigshell-cairo-pango (merged panel + dock)" ;;
    13) MODE="zigshell-blend2d";       MODE_DESC="zigshell-blend2d (merged panel + dock, Blend2D)" ;;
    *)  MODE="zigshell-blend2d";       MODE_DESC="zigshell-blend2d (merged panel + dock, Blend2D)" ;;
    *)  MODE="zigshell-cairo-pango";   MODE_DESC="zigshell-cairo-pango (merged panel + dock)" ;;
esac

echo -e "  Selected: ${GREEN}${MODE_DESC}${NC}"

echo -e "\n  ${CYAN}Launcher:${NC}  1) fuzzel (default)  2) rofi"
echo -n "  Choice [1-2]: "
if [ "${NONINTERACTIVE:-0}" = "1" ]; then
    echo "1 (auto)"
    launcher_choice="1"
else
    read -r launcher_choice
fi

case "${launcher_choice:-1}" in
    2) LAUNCHER="rofi" ;;
    *) LAUNCHER="fuzzel" ;;
esac

echo -e "  Terminal: ${GREEN}foot${NC}"

echo -e "\n  ${CYAN}Extras:${NC}"
if [ "${NONINTERACTIVE:-0}" = "1" ]; then
    echo "    Skipping extras (auto)"
    tmux_choice="N"
    nvim_choice="N"
    posh_choice="N"
    mcp_choice="N"
    opencode_choice="N"
else
    echo -n "    Tmux theme? [y/N]: " && read -r tmux_choice
    echo -n "    Neovim config? [y/N]: " && read -r nvim_choice
    echo -n "    Oh My Posh (prompt)? [y/N]: " && read -r posh_choice
    echo -n "    Antigravity CLI + MCP? [y/N]: " && read -r mcp_choice
    echo -n "    OpenCode CLI + MCP? [y/N]: " && read -r opencode_choice
fi

USE_TMUX="${tmux_choice:-N}"
USE_NVIM="${nvim_choice:-N}"
USE_POSH="${posh_choice:-N}"
USE_MCP="${mcp_choice:-N}"
USE_OPENCODE="${opencode_choice:-N}"

# Normalize to booleans
[[ "$USE_TMUX" =~ ^[Yy]$ ]] && USE_TMUX=true || USE_TMUX=false
[[ "$USE_NVIM" =~ ^[Yy]$ ]] && USE_NVIM=true || USE_NVIM=false
[[ "$USE_POSH" =~ ^[Yy]$ ]] && USE_POSH=true || USE_POSH=false
[[ "$USE_MCP" =~ ^[Yy]$ ]] && USE_MCP=true || USE_MCP=false
[[ "$USE_OPENCODE" =~ ^[Yy]$ ]] && USE_OPENCODE=true || USE_OPENCODE=false

# -------------------------------------------------------------------
# Stage 3 — Mode-Aware Dependency Resolution
# -------------------------------------------------------------------

# Determine which engines are needed for this selection
CORE_ENGINES="labwc $LAUNCHER"
SHELL_ENGINE=""
case "$MODE" in
    dms)        SHELL_ENGINE="dms" ;;
    noctalia)   SHELL_ENGINE="zigshell-cairo-pango" ;;
    tworow|lxqt-classic|lxqt-minimal|lxqt-standalone|lxqt-dual-lxqt|lxqt-vertical|lxqt-bottom)
                SHELL_ENGINE="lxqt-panel" ;;
    zigshell-cairo-pango)
        # Custom panel/dock; needs no external shell engine (zigshell-cairo-pango/lxqt).
        SHELL_ENGINE=""
        ;;
    zigshell-blend2d)
        # Custom panel/dock; needs no external shell engine.
        SHELL_ENGINE=""
        ;;
esac

# Self-contained shells — drop from the core engine list.
if [[ "$MODE" == zigshell-cairo-pango || "$MODE" == zigshell-blend2d ]]; then
    CORE_ENGINES="labwc $LAUNCHER"
fi

# Check current status of each engine
check_status() {
    command -v "$1" >/dev/null 2>&1 && echo "✓" || echo "✗"
}

echo -e "\n  ${CYAN}Engine:${NC} labwc=$(check_status labwc) $LAUNCHER=$(check_status $LAUNCHER)"
if [ -n "$SHELL_ENGINE" ]; then
    echo -e "          $SHELL_ENGINE=$(check_status $SHELL_ENGINE)"
fi

echo -e "\n  ${CYAN}Install:${NC}  1) auto-setup (packages + build)  2) configs only"
echo -n "  Choice [1-2]: "
read -r dep_choice

case "${dep_choice:-1}" in
    1)
        # Auto-setup: distro packages first, then community builds
        if [ -f "${SCRIPT_DIR}/install-distribution.sh" ]; then
            echo -e "\n${CYAN}==>${NC} Installing packages from distro repos..."
            bash "${SCRIPT_DIR}/install-distribution.sh" || echo -e "  ${YELLOW}⚠${NC} Package install had issues, continuing..."
        else
            echo -e "\n${YELLOW}⚠${NC} Distro installer not found. Install packages manually:"
            echo -e "    ./install-distribution.sh"
            echo -e "    or see docs/distro-packages.md"
        fi

        # Mode-specific builds
        need_build=""
        if [ "$MODE" = "dms" ]; then
            ! command -v dms >/dev/null 2>&1 && need_build="$need_build dms"
        elif [[ "$MODE" == tworow || "$MODE" == lxqt-* ]]; then
            ! command -v lxqt-panel >/dev/null 2>&1 && need_build="$need_build lxqt-panel"
        elif [ "$MODE" = "zigshell-cairo-pango" ]; then
            ! command -v zig >/dev/null 2>&1 && need_build="$need_build zig"
            ! command -v zigshell-cairo-pango >/dev/null 2>&1 && need_build="$need_build zigshell-cairo-pango"
        elif [ "$MODE" = "zigshell-blend2d" ]; then
            ! command -v zig >/dev/null 2>&1 && need_build="$need_build zig"
            ! command -v zigshell-blend2d >/dev/null 2>&1 && need_build="$need_build zigshell-blend2d"
        fi

        if [ -n "$need_build" ]; then
            echo -e "\n${YELLOW}⚠${NC} Unfound engines:${need_build}"
            echo -n "  Build them from source now? [y/N]: "
            read -r build_now
            if [[ "$build_now" =~ ^[Yy]$ ]]; then
                for engine in $need_build; do
                    case "$engine" in
                        dms)
                            [ -f "${SCRIPT_DIR}/build-ocws-core.sh" ] \
                                && bash "${SCRIPT_DIR}/build-ocws-core.sh" dms \
                                || echo -e "  dms:    git clone --depth=1 https://github.com/DankShrine/dms.git && cd dms && make && sudo make install"
                            ;;
                        lxqt-panel)
                            if [ -f "${SCRIPT_DIR}/install-lxqt-panel-source.sh" ]; then
                                bash "${SCRIPT_DIR}/install-lxqt-panel-source.sh"
                            else
                                echo -e "  lxqt-panel: run ./install-lxqt-panel-source.sh"
                            fi
                            ;;
                        zigshell-cairo-pango)
                            ZIG_DOCK_DIR="${SCRIPT_DIR}/src/shells/zigshell-cairo-pango"
                            if [ -f "$ZIG_DOCK_DIR/build.zig" ]; then
                                info "Building zigshell-cairo-pango..."
                                ( cd "$ZIG_DOCK_DIR" && zig build ) \
                                    && pass "Zig build complete" \
                                    || warn "Zig build failed — run: cd src/shells/zigshell-cairo-pango && zig build"
                            else
                                echo -e "  zigshell-cairo-pango: source not found at src/shells/zigshell-cairo-pango/"
                            fi
                            ;;
                        zigshell-blend2d)
                            ZIG_BLEND_DIR="${SCRIPT_DIR}/src/shells/zigshell-blend2d"
                            if [ -f "$ZIG_BLEND_DIR/build.zig" ]; then
                                info "Building zigshell-blend2d..."
                                ( cd "$ZIG_BLEND_DIR" && zig build ) \
                                    && pass "Zig build complete" \
                                    || warn "Zig build failed — run: cd src/shells/zigshell-blend2d && zig build"
                            else
                                echo -e "  zigshell-blend2d: source not found at src/shells/zigshell-blend2d/"
                            fi
                            ;;
                            zig)
                                echo -e "  zig: install from https://ziglang.org/download/"
                            ;;
                    esac
                done
            else
                for engine in $need_build; do
                    case "$engine" in
                        dms)
                            echo -e "  dms:    git clone --depth=1 https://github.com/DankShrine/dms.git && cd dms && make && sudo make install"
                            ;;
                        lxqt-panel)
                            echo -e "  lxqt-panel: ./install-lxqt-panel-source.sh"
                            ;;
                        zigshell-cairo-pango)
                            echo -e "  zigshell-cairo-pango: cd src/shells/zigshell-cairo-pango && zig build"
                            ;;
                        zigshell-blend2d)
                            echo -e "  zigshell-blend2d: cd src/shells/zigshell-blend2d && zig build"
                            ;;
                            zig)
                                echo -e "  zig: install from https://ziglang.org/download/"
                            ;;
                    esac
                done
            fi
        fi
        ;;
    *)
        echo -e "\n${YELLOW}⚠${NC} Skipping dependency installation."
        echo -e "  Required engines you'll need to install manually:"
        for engine in labwc $LAUNCHER $TERMINAL $SHELL_ENGINE; do
            [ -n "$engine" ] && ! command -v "$engine" >/dev/null 2>&1 && echo -e "    - $engine"
        done
        echo -e "  See: docs/distro-packages.md"
        ;;
esac

# -------------------------------------------------------------------
# MCP Setup: Antigravity CLI + codebase-memory-mcp
# -------------------------------------------------------------------
install_mcp_tools() {
    info "Installing Antigravity CLI..."
    if command -v agy &>/dev/null; then
        pass "Antigravity CLI already installed ($(agy --version 2>/dev/null || true))"
    else
        TEMP_INSTALLER=$(mktemp /tmp/antigravity-install-XXXXXX.sh)
        if curl -fsSL -o "$TEMP_INSTALLER" https://antigravity.google/cli/install.sh; then
            bash "$TEMP_INSTALLER" 2>&1 || warn "Antigravity CLI install had issues"
            rm -f "$TEMP_INSTALLER"
        else
            warn "Failed to download antigravity installer"
        fi
        if command -v agy &>/dev/null; then
            pass "Antigravity CLI installed"
        fi
    fi

    info "Installing codebase-memory-mcp..."
    if command -v codebase-memory-mcp &>/dev/null; then
        pass "codebase-memory-mcp already installed ($(codebase-memory-mcp --version 2>/dev/null || true))"
    else
        TEMP_INSTALLER=$(mktemp /tmp/codebase-memory-mcp-install-XXXXXX.sh)
        if curl -fsSL -o "$TEMP_INSTALLER" https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/install.sh; then
            bash "$TEMP_INSTALLER" 2>&1 || warn "codebase-memory-mcp install had issues"
            rm -f "$TEMP_INSTALLER"
        else
            warn "Failed to download codebase-memory-mcp installer"
        fi
        if command -v codebase-memory-mcp &>/dev/null; then
            pass "codebase-memory-mcp installed"
        fi
    fi

    local agy_config="$HOME/.gemini/config/mcp_config.json"
    if [ -f "$agy_config" ] && grep -q codebase-memory-mcp "$agy_config" 2>/dev/null; then
        pass "codebase-memory-mcp pre-configured for Antigravity CLI"
    else
        warn "codebase-memory-mcp not auto-detected for Antigravity. Run: codebase-memory-mcp install"
    fi
}

install_opencode_mcp() {
    info "Installing OpenCode CLI..."
    if command -v opencode &>/dev/null; then
        pass "OpenCode CLI already installed ($(opencode --version 2>/dev/null || true))"
    else
        TEMP_INSTALLER=$(mktemp /tmp/opencode-install-XXXXXX.sh)
        if curl -fsSL -o "$TEMP_INSTALLER" https://opencode.ai/install; then
            bash "$TEMP_INSTALLER" 2>&1 || warn "OpenCode CLI install had issues"
            rm -f "$TEMP_INSTALLER"
        else
            warn "Failed to download opencode installer"
        fi
        if command -v opencode &>/dev/null; then
            pass "OpenCode CLI installed"
        fi
    fi

    info "Installing codebase-memory-mcp..."
    if command -v codebase-memory-mcp &>/dev/null; then
        pass "codebase-memory-mcp already installed ($(codebase-memory-mcp --version 2>/dev/null || true))"
    else
        TEMP_INSTALLER=$(mktemp /tmp/codebase-memory-mcp-install-XXXXXX.sh)
        if curl -fsSL -o "$TEMP_INSTALLER" https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/install.sh; then
            bash "$TEMP_INSTALLER" 2>&1 || warn "codebase-memory-mcp install had issues"
            rm -f "$TEMP_INSTALLER"
        else
            warn "Failed to download codebase-memory-mcp installer"
        fi
        if command -v codebase-memory-mcp &>/dev/null; then
            pass "codebase-memory-mcp installed"
        fi
    fi

    local opencode_config="${XDG_CONFIG_HOME:-$HOME/.config}/opencode/opencode.json"
    if [ -f "$opencode_config" ] && grep -q codebase-memory-mcp "$opencode_config" 2>/dev/null; then
        pass "codebase-memory-mcp pre-configured for OpenCode CLI"
    else
        warn "codebase-memory-mcp not auto-detected for OpenCode. Run: codebase-memory-mcp install"
    fi
}

if [ "$USE_MCP" = true ]; then
    case "${dep_choice:-1}" in
        1) install_mcp_tools ;;
        *)
            echo -e "\n${YELLOW}⚠${NC} Skipping MCP tools install (configs-only mode)."
            echo "  To install later:"
            echo "    curl -fsSL https://antigravity.google/cli/install.sh | bash"
            echo "    curl -fsSL https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/install.sh | bash"
            ;;
    esac
fi

if [ "$USE_OPENCODE" = true ]; then
    case "${dep_choice:-1}" in
        1) install_opencode_mcp ;;
        *)
            echo -e "\n${YELLOW}⚠${NC} Skipping OpenCode tools install (configs-only mode)."
            echo "  To install later:"
            echo "    curl -fsSL https://opencode.ai/install | bash"
            echo "    curl -fsSL https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/install.sh | bash"
            ;;
    esac
fi

# -------------------------------------------------------------------
# Oh My Posh — cross-platform prompt prompt
# -------------------------------------------------------------------
install_oh_my_posh() {
    info "Installing Oh My Posh..."
    if command -v oh-my-posh &>/dev/null; then
        local omp_ver
        omp_ver=$(oh-my-posh --version 2>/dev/null | head -1)
        pass "Oh My Posh already installed ($omp_ver)"
    else
        echo -e "  ${CYAN}Oh My Posh${NC} is a cross-platform prompt theme engine for bash/zsh/fish."
        echo -e "  It shows git status, execution time, error codes, and more in your terminal."
        echo -e "  ${DIM}https://github.com/jandedobbeleer/oh-my-posh${NC}"
        echo ""
        echo -n "  Install Oh My Posh now? [Y/n]: "
        read -r omp_confirm
        if [[ "${omp_confirm:-Y}" =~ ^[Nn]$ ]]; then
            warn "Skipping Oh My Posh install."
            return 0
        fi

        # Install via the official install script
        echo -e "  ${DIM}Downloading oh-my-posh...${NC}"
        TEMP_INSTALLER=$(mktemp /tmp/ohmyposh-install-XXXXXX.sh)
        if curl -fsSL -o "$TEMP_INSTALLER" https://ohmyposh.dev/install.sh; then
            bash "$TEMP_INSTALLER" 2>&1 | tail -5 || warn "oh-my-posh installation failed"
            rm -f "$TEMP_INSTALLER"
        else
            warn "Failed to download oh-my-posh installer"
        fi

        # Verify install
        if command -v oh-my-posh &>/dev/null; then
            local omp_ver
            omp_ver=$(oh-my-posh --version 2>/dev/null | head -1)
            pass "Oh My Posh installed ($omp_ver)"
        else
            # Check common install locations
            for dir in "$HOME/.local/bin" "$HOME/.oh-my-posh/bin" "/usr/local/bin"; do
                if [ -x "$dir/oh-my-posh" ]; then
                    export PATH="$dir:$PATH"
                    pass "Oh My Posh installed at $dir"
                    break
                fi
            done
            if ! command -v oh-my-posh &>/dev/null; then
                warn "Oh My Posh installed but not found in PATH. You may need to restart your shell."
            fi
        fi
    fi

    # Deploy config
    local omp_config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/oh-my-posh"
    local omp_theme="catppuccin-mocha"

    mkdir -p "$omp_config_dir"
    if [ -f "$SCRIPT_DIR/dotfiles/oh-my-posh/${omp_theme}.omp.json" ]; then
        cp "$SCRIPT_DIR/dotfiles/oh-my-posh/${omp_theme}.omp.json" "$omp_config_dir/${omp_theme}.omp.json"
        pass "Oh My Posh theme deployed: $omp_theme"
    fi

    # Create shell init snippet
    local bash_init="$omp_config_dir/omp-init.sh"
    cat > "$bash_init" << 'SHELLEOF'
# Oh My Posh — initialize prompt (added by OCWS installer)
if command -v oh-my-posh &>/dev/null; then
    OMP_CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/oh-my-posh/catppuccin-mocha.omp.json"
    if [ -f "$OMP_CONFIG" ]; then
        eval "$(oh-my-posh init bash --config "$OMP_CONFIG")"
    fi
fi
SHELLEOF
    pass "Shell init snippet: $bash_init"

    # Add to .bashrc if not already present
    local bashrc="$HOME/.bashrc"
    local omp_marker="# Oh My Posh (OCWS)"
    if [ -f "$bashrc" ] && grep -q "$omp_marker" "$bashrc"; then
        pass "Oh My Posh already configured in .bashrc"
    elif [ -f "$bashrc" ]; then
        echo "" >> "$bashrc"
        echo "$omp_marker" >> "$bashrc"
        echo "[ -f \"$bash_init\" ] && source \"$bash_init\"" >> "$bashrc"
        pass "Oh My Posh added to .bashrc"
    else
        warn ".bashrc not found — add this to your shell init:"
        echo "    [ -f \"$bash_init\" ] && source \"$bash_init\""
    fi

    echo -e "\n  ${GREEN}Oh My Posh setup complete!${NC}"
    echo -e "  Restart your shell or run: ${CYAN}source ~/.bashrc${NC}"
    echo -e "  Customize: ${CYAN}oh-my-posh init bash --config ~/.config/oh-my-posh/catppuccin-mocha.omp.json${NC}"
}

if [ "$USE_POSH" = true ]; then
    case "${dep_choice:-1}" in
        1) install_oh_my_posh ;;
        *)
            echo -e "\n${YELLOW}⚠${NC} Skipping Oh My Posh install (configs-only mode)."
            echo "  To install later:"
            echo "    curl -fsSL https://ohmyposh.dev/install.sh | bash"
            echo "  Config already deployed to ~/.config/oh-my-posh/"
            ;;
    esac
fi

# -------------------------------------------------------------------
# Deploy Configurations
# -------------------------------------------------------------------

# Pre-deploy confirmation
echo -e "\n  ${CYAN}Deploy:${NC} $MODE_DESC | $LAUNCHER | foot"
echo -e "  ${CYAN}Extras:${NC} tmux=$USE_TMUX nvim=$USE_NVIM posh=$USE_POSH mcp=$USE_MCP opencode=$USE_OPENCODE"
echo -e "  ${YELLOW}Target: ~/.config/ ~/.local/bin/${NC}"

echo -n "  Deploy? [y/N]: "
read -r confirm
if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    echo -e "\n  Aborted."
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
    dms)         mkdir -p ~/.local/share/quickshell/dms
                 mkdir -p ~/.config/quickshell/dms ;;
    noctalia)    mkdir -p ~/.config/noctalia ;;
esac
pass "Directories created."

# --- Disable GNOME Keyring "login keyring" popup ---
# Removes the old keyring so it's recreated without a password prompt.
# The labwc environment file also sets GNOME_KEYRING_CONTROL=unset to
# prevent D-Bus/systemd from auto-starting gnome-keyring-daemon.
info "Disabling GNOME Keyring login popup..."
KEYRING_DIR="$HOME/.local/share/keyrings"
KEYRING_FILE="$KEYRING_DIR/login.keyring"
KEYRING_CFG="$KEYRING_DIR/login.keyring.cfg"

if [ -d "$KEYRING_DIR" ] && [ -f "$KEYRING_FILE" ]; then
    # Backup and remove old keyring — will be recreated with empty password
    BAK_DIR="$KEYRING_DIR.bak.$(date +%Y%m%d%H%M%S)"
    mkdir -p "$BAK_DIR"
    cp "$KEYRING_DIR"/login.keyring* "$BAK_DIR/" 2>/dev/null || true
    rm -f "$KEYRING_FILE" "$KEYRING_CFG" 2>/dev/null || true
    pass "Old keyring removed (will auto-create without password popup)."
else
    pass "No existing keyring to fix."
fi

# 3. Deploy Labwc Core
info "Deploying Compositor Rules (labwc)..."
cp -r "$SCRIPT_DIR/dotfiles/labwc/"* ~/.config/labwc/ 2>/dev/null || fail "Failed to deploy labwc configurations"
pass "labwc configurations synced."

# 3b. Deploy LXQt Panel config (used by the tworow shell mode: lxqt-panel on top)
if [ -d "$SCRIPT_DIR/dotfiles/lxqt" ]; then
    info "Deploying LXQt Panel configuration..."
    mkdir -p ~/.config/lxqt
    cp -r "$SCRIPT_DIR/dotfiles/lxqt/"* ~/.config/lxqt/ 2>/dev/null || true
    pass "LXQt Panel configuration synced."
fi

# 3b2. Deploy wallpaper sources for the wallpaper script
if [ -f "$SCRIPT_DIR/dotfiles/wallpaper-sources.txt" ]; then
    mkdir -p ~/.config/ocws
    cp "$SCRIPT_DIR/dotfiles/wallpaper-sources.txt" ~/.config/ocws/wallpaper-sources.txt 2>/dev/null || true
    pass "Wallpaper sources synced."
fi

# 3c. Wire runtime scripts into the labwc session PATH
# rc.xml keybinds/menus call scripts (theme, labwc-theme, shell-mode-picker.sh,
# actions.sh, shell-switcher.sh, workspace-actions.sh, wallpaper, ...). These
# scripts use $SCRIPT_DIR-relative paths (lib/, theme-engine.sh, ../themes), so
# they must run from the repo — we add the repo scripts dirs to the labwc
# session PATH instead of copying them (copying would break those paths).
info "Wiring runtime scripts into PATH..."
ENV_FILE="$HOME/.config/labwc/environment"
if [ -f "$ENV_FILE" ]; then
    # Remove any previously injected line (idempotent on re-install)
    sed -i '/# OCWS runtime scripts (injected by install.sh)/d' "$ENV_FILE"
    # Absolute PATH built from the current one so normal dirs are preserved
    NEW_PATH="${PATH}:$HOME/.local/bin:$SCRIPT_DIR/scripts:$SCRIPT_DIR/scripts/actions"
    printf '\n# OCWS runtime scripts (injected by install.sh)\nPATH=%s\n' "$NEW_PATH" >> "$ENV_FILE"
fi
# actions.sh dispatches to modules in ~/.local/bin/actions/ — symlink them from
# the repo so the dispatcher keeps working.
mkdir -p ~/.local/bin/actions
ln -sf "$SCRIPT_DIR"/scripts/actions/*.sh ~/.local/bin/actions/ 2>/dev/null || true
# Standalone wallpaper helper referenced by rc.xml (dotfiles/wallpaper)
if [ -f "$SCRIPT_DIR/dotfiles/wallpaper" ]; then
    cp "$SCRIPT_DIR/dotfiles/wallpaper" ~/.local/bin/wallpaper 2>/dev/null && chmod +x ~/.local/bin/wallpaper || true
fi
# Panel mode cycle script referenced by rc.xml (Super+p)
for script in panel-mode-cycle lxqt-panel-switcher; do
    if [ -f "$SCRIPT_DIR/scripts/$script" ]; then
        cp "$SCRIPT_DIR/scripts/$script" "$HOME/.local/bin/$script" 2>/dev/null && chmod +x "$HOME/.local/bin/$script" || true
    fi
done
pass "Runtime scripts wired into PATH."

# 4. Deploy OCWS Shell (supporting infrastructure for all modes)
info "Deploying the OCWS Shell..."
case "$MODE" in
    doublepanel|tworow|minimal|lxqt-classic|lxqt-minimal|lxqt-vertical|lxqt-bottom)
        # Full OCWS bar config — includes modes/ and bars/ (required by
        # tworow/minimal so their .mode files and bar layouts deploy).
        rsync -a --exclude='user.config' "$SCRIPT_DIR/dotfiles/ocws/" ~/.config/ocws/ 2>/dev/null || cp -r "$SCRIPT_DIR/dotfiles/ocws/"* ~/.config/ocws/ 2>/dev/null || fail "Failed to deploy OCWS shell"
        ;;
    *)
        # Supporting infrastructure only — no zigshell-cairo-pango bar config at all
        rsync -a --exclude='user.config' --exclude='modes/' --exclude='bars/' --exclude='css/ocws.css' "$SCRIPT_DIR/dotfiles/ocws/" ~/.config/ocws/ 2>/dev/null || cp -r "$SCRIPT_DIR/dotfiles/ocws/"* ~/.config/ocws/ 2>/dev/null || fail "Failed to deploy OCWS shell"
        ;;
esac

if [ ! -f ~/.config/ocws/user.config ]; then
    cp "$SCRIPT_DIR/dotfiles/ocws/user.config" ~/.config/ocws/user.config 2>/dev/null || true
fi

echo "$MODE" > ~/.config/ocws/mode
echo "$LAUNCHER" > ~/.config/ocws/launcher
echo "$TERMINAL" > ~/.config/ocws/terminal
pass "OCWS infrastructure, mode ($MODE), launcher ($LAUNCHER), and terminal ($TERMINAL) synced."

# 5. Deploy shell-specific configs
case "$MODE" in
    dms)
        if [ -d "$SCRIPT_DIR/dotfiles/DankMaterialShell" ]; then
            info "Deploying Dank Material Shell configuration..."
            # DMS defaults to ~/.config/quickshell/dms/ for its QML shell files.
            # Quickshell's module scanner can't follow symlinks, so we must copy.
            mkdir -p ~/.local/share/quickshell/dms
            mkdir -p ~/.config/quickshell
            rm -rf ~/.config/quickshell/dms
            cp -r ~/.local/share/quickshell/dms ~/.config/quickshell/dms
            # Deploy user settings into the config dir
            rsync -a "$SCRIPT_DIR/dotfiles/DankMaterialShell/" ~/.config/quickshell/dms/ 2>/dev/null || true
            # Strip unsupported pragmas for older quickshell versions
            DMS_SHELL="$HOME/.config/quickshell/dms/shell.qml"
            if [ -f "$DMS_SHELL" ]; then
                sed -i '/^\/\/@ pragma AppId /d' "$DMS_SHELL"
            fi
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
    zigshell-cairo-pango)
        info "Deploying zigshell-cairo-pango..."
        ZIG_DOCK_DIR="$SCRIPT_DIR/src/shells/zigshell-cairo-pango"
        # Build with zig and install the merged binary to ~/.local/bin
        if [ -f "$ZIG_DOCK_DIR/build.zig" ]; then
            ( cd "$ZIG_DOCK_DIR" && zig build ) \
                || warn "Zig build failed — run: cd src/shells/zigshell-cairo-pango && zig build"
            if [ -f "$ZIG_DOCK_DIR/zig-out/bin/zigshell-cairo-pango" ]; then
                install -Dm755 "$ZIG_DOCK_DIR/zig-out/bin/zigshell-cairo-pango" ~/.local/bin/zigshell-cairo-pango
                pass "zigshell-cairo-pango installed."
            fi
        else
            warn "Zig source not found at src/shells/zigshell-cairo-pango/"
        fi
        pass "zigshell-cairo-pango installed; launched by labwc autostart when mode=zigshell-cairo-pango."
        ;;
    zigshell-blend2d)
        info "Deploying zigshell-blend2d..."
        ZIG_BLEND_DIR="$SCRIPT_DIR/src/shells/zigshell-blend2d"
        # Build with zig and install the merged binary to ~/.local/bin
        if [ -f "$ZIG_BLEND_DIR/build.zig" ]; then
            ( cd "$ZIG_BLEND_DIR" && zig build ) \
                || warn "Zig build failed — run: cd src/shells/zigshell-blend2d && zig build"
            if [ -f "$ZIG_BLEND_DIR/zig-out/bin/zigshell-blend2d" ]; then
                install -Dm755 "$ZIG_BLEND_DIR/zig-out/bin/zigshell-blend2d" ~/.local/bin/zigshell-blend2d
                # Install Blend2D shared library
                if [ -f "$ZIG_BLEND_DIR/build/deps/blend2d/libblend2d.so" ]; then
                    install -Dm755 "$ZIG_BLEND_DIR/build/deps/blend2d/libblend2d.so" ~/.local/lib/libblend2d.so
                    pass "libblend2d.so installed."
                fi
                # Ensure LD_LIBRARY_PATH includes ~/.local/lib
                BASHRC="$HOME/.bashrc"
                if [ -f "$BASHRC" ] && ! grep -q '\.local/lib' "$BASHRC" 2>/dev/null; then
                    echo 'export LD_LIBRARY_PATH="$HOME/.local/lib:${LD_LIBRARY_PATH:-}"' >> "$BASHRC"
                    pass "LD_LIBRARY_PATH added to .bashrc"
                fi
                pass "zigshell-blend2d installed."
            fi
        else
            warn "Zig source not found at src/shells/zigshell-blend2d/"
        fi
        pass "zigshell-blend2d installed; launched by labwc autostart when mode=zigshell-blend2d."
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
        info "Deploying Fuzzel configuration..."
        mkdir -p ~/.config/fuzzel
        cp -r "$SCRIPT_DIR/dotfiles/fuzzel/"* ~/.config/fuzzel/ 2>/dev/null || true
        pass "Fuzzel synced."
        ;;
esac

# Deploy Terminal
case "$TERMINAL" in
    foot)
        if [ -d "$SCRIPT_DIR/dotfiles/foot" ]; then
            info "Deploying Foot Terminal configuration..."
            mkdir -p ~/.config/foot
            cp -r "$SCRIPT_DIR/dotfiles/foot/"* ~/.config/foot/ 2>/dev/null || true
            pass "Foot synced."
        fi
        ;;

esac

# Deploy Tmux
if [ "$USE_TMUX" = true ]; then
    info "Deploying OCWS Tmux configuration..."
    if [ -f ~/.tmux.conf ]; then
        cp ~/.tmux.conf "$SCRIPT_DIR/dotfiles/tmux/tmux.conf.bak" 2>/dev/null || true
        pass "Existing tmux.conf backed up to dotfiles/tmux/tmux.conf.bak"
    fi
    mkdir -p ~/.config/tmux
    if [ -f "$SCRIPT_DIR/dotfiles/tmux/tmux.conf" ]; then
        cp "$SCRIPT_DIR/dotfiles/tmux/tmux.conf" ~/.tmux.conf 2>/dev/null || true
        pass "OCWS tmux.conf deployed."
    fi
fi

# Deploy Neovim (modular structure)
if [ "$USE_NVIM" = true ]; then
    info "Deploying OCWS Neovim configuration..."
    if [ -d ~/.config/nvim ]; then
        cp -r ~/.config/nvim "$SCRIPT_DIR/dotfiles/nvim.bak" 2>/dev/null || true
        pass "Existing nvim config backed up to dotfiles/nvim.bak"
    fi
    mkdir -p ~/.config/nvim
    if [ -d "$SCRIPT_DIR/dotfiles/nvim" ]; then
        cp -r "$SCRIPT_DIR/dotfiles/nvim"/* ~/.config/nvim/ 2>/dev/null || true
        pass "OCWS nvim/ deployed (modular)."
    fi
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

# 8. Deploy Compiled C Binaries
info "Deploying compiled OCWS binaries..."
if [ -d "$SCRIPT_DIR/zig-out/bin" ]; then
    mkdir -p ~/.local/bin
    cp -r "$SCRIPT_DIR/zig-out/bin/"* ~/.local/bin/ 2>/dev/null || true
    
    # Ensure they are executable
    chmod +x ~/.local/bin/ocws-* 2>/dev/null || true
    
    pass "Compiled C binaries installed to ~/.local/bin/."
else
    warn "zig-out/bin not found. Run 'zig build' first if you want C utilities installed."
fi

echo -e "\n${GREEN}OCWS Installation Complete!${NC}"
echo -e "You may need to log out and log back in, or restart your Wayland compositor."

