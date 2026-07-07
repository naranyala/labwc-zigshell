# Getting Started with OCWS

This guide covers installation, first-run usage, and troubleshooting for OCWS (Our C-Written Shell) — a Wayland desktop environment built on `labwc`, `sfwbar`, and `fuzzel` using only C and GTK3.

---

## Architecture Overview

OCWS is built on four layers:

| Layer | Component | Role |
|-------|-----------|------|
| Compositor | `labwc` | Wayland session, window management, input, keybindings |
| Shell UI | `sfwbar` | GTK3 panel engine: widgets, tray, taskbar, popups |
| Launcher | `fuzzel` | App launcher and dmenu-mode script runner |
| Layer Shell | `gtk-layer-shell` | Anchors shell surfaces to Wayland outputs |

Supporting services: `ocws-notify` (notifications), `swayidle` + `swaylock` (idle/lock), `cliphist` + `wl-clipboard` (clipboard), `playerctl` (media), `ocws-brightness` (backlight), `gammastep` (night light), `grim` + `slurp` (screenshots).

---

## Installation

### Step 1: Install Dependencies

On **Arch Linux**:

```bash
sudo pacman -S labwc sfwbar fuzzel gtk-layer-shell pipewire wireplumber libpulse \
  inotify-tools playerctl bc wl-clipboard cliphist \
  polkit-gnome swayidle swaylock grim slurp foot tesseract leptonica
```

On **Debian/Ubuntu** or **Fedora**, see `distro/debian.sh` and `distro/fedora.sh`.

To compile the latest upstream versions from source:

```bash
./build-ocws-core.sh all
```

### Step 2: Run the Installer

```bash
git clone <this-repo> && cd labwc-fuzzel-sfwbar
./install.sh
```

The installer:

1. Checks system dependencies
2. Backs up any existing `~/.config/labwc/` and `~/.config/ocws/`
3. Deploys `dotfiles/labwc/` to `~/.config/labwc/`
4. Deploys `dotfiles/ocws/` to `~/.config/ocws/`
5. Deploys `dotfiles/fuzzel/` to `~/.config/fuzzel/`
6. Deploys GTK settings to `~/.config/gtk-3.0/` and `~/.config/gtk-4.0/`
7. Links all scripts from `scripts/` to `~/.local/bin/`
8. Links action scripts from `scripts/actions/` to `~/.local/bin/actions/`
9. Installs built C binaries from `zig-out/bin/` to `~/.local/bin/`

### Step 3: Launch the Session

**From a display manager** (GDM, SDDM, ly): log out and select **labwc**.

**From a TTY**:

```bash
labwc
```

---

## Installed File Layout

| Path | Contents |
|------|----------|
| `~/.config/labwc/` | `rc.xml`, `menu.xml`, `autostart`, `environment`, `themerc-override` |
| `~/.config/ocws/` | `ocws.config`, `*.widget`, `ocws-daemon.sh`, `plugins/`, `state.kv` |
| `~/.config/fuzzel/` | `fuzzel.ini` |
| `~/.config/foot/` | `foot.ini` |
| `~/.local/bin/` | All `scripts/*.sh` and C helper binaries (`ocws-*`) |
| `~/.local/bin/actions/` | All `scripts/actions/*.sh` |

---

## What Starts at Boot

`~/.config/labwc/autostart` runs automatically when labwc starts. Key services launched:

| Service | Command | Role |
|---------|---------|------|
| Wallpaper | `ocws-wallpaper ~/Pictures/wallpapers/` | Time-of-day wallpaper transitions |
| Shell UI | `sfwbar` | Native GTK3 OCWS Interface |
| OCWS Daemon | `~/.config/ocws/ocws-daemon.sh` | Event Bus IPC listener |
| Notifications | `ocws-notify` | D-Bus notification daemon (replaces mako) |
| Clipboard | `wl-paste --watch cliphist store` | Clipboard history daemon |
| Idle/Lock | `swayidle -w timeout 300 'swaylock -f'` | Auto-lock after 5 min |
| Night light | `gammastep -t 6500:3500 -g 1.0 -r` | Day/night color temp |

---

## Default Keybindings

Keybindings are defined in `~/.config/labwc/rc.xml`.

### Application Keybindings

| Key | Action |
|-----|--------|
| `Super+Enter` | Launch terminal (`foot`) |
| `Super+D` | Launch app launcher (`fuzzel`) |
| `Super+V` | Open clipboard history (`cliphist` + `fuzzel`) |
| `Super+Q` | Close focused window |
| `Super+F` | Toggle fullscreen |

### Workspace Keybindings

| Key | Action |
|-----|--------|
| `Super+1-9` | Switch to workspace 1-9 |
| `Super+Shift+1-9` | Move window to workspace 1-9 |
| `Alt+Tab` | Cycle through windows |

### System Keybindings

| Key | Action |
|-----|--------|
| `XF86AudioRaiseVolume` | Volume up |
| `XF86AudioLowerVolume` | Volume down |
| `XF86AudioMute` | Toggle mute |
| `XF86MonBrightnessUp` | Brightness up |
| `XF86MonBrightnessDown` | Brightness down |
| `Print` | Screenshot region to file |
| `Super+Print` | Screenshot fullscreen to file |
| `Shift+Print` | Screenshot region to clipboard |

---

## Using the Shell

### Shell Modes

OCWS provides multiple desktop paradigms through its modular configuration. You can switch between them on the fly:

```bash
# Interactive UI
shell-mode-picker.sh

# CLI
toggle-shell doublepanel   # OCWS dual-panel (default)
toggle-shell crystaldock   # sfwbar statusbar + crystal-dock
toggle-shell minimal       # minimal sfwbar (clock, volume, battery)
toggle-shell dms           # DankMaterialShell
toggle-shell noctalia      # Noctalia shell

# Using the new mode selector
sfwbar-mode start doublepanel
sfwbar-mode start crystaldock
sfwbar-mode start minimal
sfwbar-mode status
sfwbar-mode validate
```

Or use the graphical settings app:

```bash
ocws-settings
```

### Control Center

Click the clock or system tray area on the panel to open the **OCWS Control Center** popup. It includes volume, brightness, battery, WiFi, Bluetooth, and media controls.

### Application Launcher

Press `Super+D` or click the launcher button in the panel to open `fuzzel`. Start typing to fuzzy-search installed apps.

### Theme Switching

```bash
# List available themes
theme-engine.sh list

# Preview a theme (live preview, reverts on Ctrl+C)
theme-engine.sh preview themes/catppuccin-mocha.ini

# Apply permanently
theme-engine.sh apply themes/catppuccin-mocha.ini
```

Available themes in `themes/`:
`catppuccin-mocha`, `tokyo-night`, `dracula`, `nord`, `rose-pine`, `gruvbox`, `everforest`, `kanagawa`, `one-dark`, `solarized-dark`, `flexoki`

### Wallpaper-Adaptive Theming

Extract palette from your current wallpaper and auto-generate a matching theme:

```bash
# Extract 6 dominant colors from wallpaper
ocws-color -n 6 -f ini ~/Pictures/wallpaper.png

# Use with theme engine
ocws-color -f scss ~/Pictures/wallpaper.png > ~/.config/ocws/palette.scss
```

### Smooth Hardware Control

All hardware controls use animated transitions (cubic easing):

```bash
ocws-brightness set 50    # Smooth fade to 50%
ocws-brightness up        # +5% with animation
ocws-volume set 75        # Smooth fade to 75%
ocws-volume up            # +5% with animation
```

### Screen OCR

Extract text from any screen region:

```bash
ocws-ocr                  # Select region, OCR to stdout
ocws-ocr -c               # Select region, OCR to clipboard
ocws-ocr screenshot.png   # OCR an image file
```

### Plugin Widgets

Drop any `.widget` file into `~/.config/ocws/plugins/` and reload the shell. The `ocws-plugin-loader.sh` auto-injects it into the running configuration.

---

## Verifying the Installation

```bash
# Check core binaries are in PATH
which labwc sfwbar fuzzel foot ocws

# Check OCWS config directories exist
ls ~/.config/ocws/
ls ~/.config/labwc/

# Check C helper binaries are installed
ls ~/.local/bin/ocws

# Test the Event Bus
ocws-emit.sh System.Volume 75
```

---

## Troubleshooting

### sfwbar panel does not start

```bash
# Run sfwbar manually to see errors
sfwbar -f ~/.config/ocws/ocws.config

# Check for missing widget includes
grep -r 'Include\|Scanner' ~/.config/ocws/ocws.config
```

### labwc will not start / black screen

```bash
# Debug labwc directly
debug-labwc.sh

# Or run with verbose output from TTY
labwc 2>&1 | tee /tmp/labwc.log
```

### Theme not applying

```bash
theme-engine.sh list
theme-engine.sh apply themes/catppuccin-mocha.ini
# Then reload labwc
labwc --reconfigure
```

### Clipboard history empty

```bash
# Ensure cliphist daemon is running
pgrep -a wl-paste
# If not running, start it
wl-paste --type text/plain --watch cliphist store &
```

### Volume/brightness keys not working

```bash
# Test scripts directly
~/.local/bin/actions/audio.sh up
~/.local/bin/actions/brightness.sh up

# Verify keybinds in rc.xml
grep -A3 'XF86Audio\|XF86MonBrightness' ~/.config/labwc/rc.xml
```

### Screenshots not saving

```bash
# Test grim directly
grim ~/Pictures/test.png && echo "OK"

# Check save directory
ls ~/Pictures/Screenshots/ 2>/dev/null || mkdir -p ~/Pictures/Screenshots
```

---

## Further Reading

- `docs/configuration.md` — Event Bus API, plugin system, CSS customization, window rules
- `docs/events.md` — Full IPC event contract with variable mappings
- `docs/lessons/` — 55 lesson files covering sfwbar internals, bugs, and patterns
- `sources-learn/labwc.md` — labwc config deep dive
- `sources-learn/sfwbar.md` — sfwbar widget DSL reference
- `sources-learn/fuzzel.md` — fuzzel config and dmenu scripting
- `dotfiles/ocws/README.md` — OCWS widget file inventory
- `TODOS.md` — Strategic roadmap with phase tracking
