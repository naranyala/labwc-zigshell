# OCWS: Our C-Written Shell

A high-performance, purely C-native Wayland desktop environment built on `labwc`, `sfwbar`, and `fuzzel`. By strictly adhering to a C and GTK3 foundation, OCWS delivers a translucent glassmorphic aesthetic with zero JavaScript, Electron, or Qt runtime overhead.

## Philosophy

OCWS stands where two lineages meet: **DankMaterialShell** proved that a Wayland desktop can feel deliberate, designed, and opinionated — a single cohesive experience instead of a pile of duct-taped configs. **Noctalia Shell** proved that a desktop can be *quiet by design* — minimal, unobtrusive, getting out of your way while still being deeply configurable.

We took both ideas seriously, then built on C and GTK3 because we believe performance is not a feature you bolt on later — it is the foundation everything else rests on. No JavaScript runtime, no Electron, no QML virtual machine. Just native code, the Wayland protocol, and a modular architecture that any programmer can understand, debug, and extend in an afternoon.

### What we drew from DankMaterialShell

DankMaterialShell showed that the Wayland ecosystem was ready for a desktop that does not ask you to assemble it from twenty independent projects. Its Material 3 fluency — dynamic color from wallpaper, consistent surfaces, deliberate spacing — proved that an opinionated visual system could coexist with deep customizability.

OCWS takes that conviction and translates it into GTK3 native widgets and C-level compositor integration:

- **Cohesive visual language**: glassmorphic surfaces, consistent radii, deliberate alpha values — all maintained across every widget through a shared token system (`tokens.css`), not duplicated per-component.
- **Opinionated defaults, no ceiling**: you get a polished desktop on install. If you want to replace every widget, rewrite the bar, or swap the compositor — the modular architecture does not fight you.
- **Theme as a first-class surface**: like DMS's matugen pipeline, OCWS has a theme engine that propagates a single palette into 11+ config surfaces — labwc themerc, sfwbar CSS, GTK settings, fuzzel, foot, rofi, mako, Qt6, and the OCWS glass CSS. Change one INI file, the whole desktop re-themes.

### What we drew from Noctalia

Noctalia's *quiet by design* ethos is the closest articulation we have found to what a desktop should *feel* like: present when you need it, invisible when you do not. Its modular plugin architecture and clean separation of concerns set a standard for maintainability.

OCWS channels this through:

- **Event Bus architecture, not a monolith**: our background daemons (`ocws-daemon.sh`) emit typed events via `ocws-emit.sh`; the UI layer subscribes via sfwbar's variable system. No tight coupling, no shared state, no IPC framework heavier than a shell script and a FIFO.
- **Widgets as independent modules**: each `.widget` file in `dotfiles/ocws/` is self-contained — its config, its CSS, its data bindings. You can delete, replace, or fork a single widget without touching anything else.
- **Compositor freedom**: OCWS does not fork labwc or embed a custom compositor. It works with labwc, and the same widget set can adapt to any wlr-layer-shell compositor. We stay out of the compositor's job.

### The OCWS commitment

1. **Zero bloat, zero apology**: glassmorphic surfaces, smooth transitions, translucent panels — delivered in C with GTK3. No JS heap, no JIT pause, no Electron process tree. A complete desktop session sits under 200 MB of RAM.

2. **Modular until it hurts, then more modular**: every panel, every widget, every daemon is a replaceable unit. The `switcher`, `pager`, `taskbar`, `tray` — all are config blocks in a text file, not compiled-in assumptions.

3. **Config as code**: sfwbar's `#Api2` config language is a declarative DSL. Widget trees, event bindings, CSS, and layout are all expressed in the same file. The boundary between "configuring" and "programming" is deliberately thin.

4. **The Unix pipeline, not the framework**: our IPC chain is `inotifywait | ocws-emit.sh | sfwbar variable`. Our theme engine is `ini parser | template renderer | file writer`. Every link in the chain is a standalone shell command you can run, test, and pipe yourself.

OCWS is not trying to be the next GNOME or KDE. It is not trying to be the most popular desktop. It is trying to be the desktop that stays out of your way, performs like native code should, and lets you reshape every pixel of it without needing a TypeScript build pipeline.

## Table of Contents

- [Architecture](#architecture)
- [Codebase Structure](#codebase-structure)
- [C Utility Binaries](#c-utility-binaries)
- [Widget System](#widget-system)
- [Event Bus (IPC)](#event-bus-ipc)
- [Theme Engine](#theme-engine)
- [Build System](#build-system)
- [Installation](#installation)
- [Keybindings](#keybindings)
- [Documentation](#documentation)

## Architecture

OCWS is a modular platform built on four layers:

| Layer | Component | Role |
|-------|-----------|------|
| Compositor | `labwc` | Wayland session, window management, input handling, keybindings |
| Shell UI | `sfwbar` / `noctalia` / `crystal-dock` | Selectable shell modes (OCWS dual-panel, Noctalia, Crystal Dock) via shell-switcher |
| Launcher | `fuzzel` | Application launcher and dmenu-mode script runner |
| Layer Shell | `gtk-layer-shell` | Anchors the shell surfaces to Wayland outputs |

Supporting services: `ocws-notify` (D-Bus notifications), `swayidle` + `swaylock` (idle/lock), `cliphist` + `wl-clipboard` (clipboard), `playerctl` (media), `ocws-brightness` (backlight), `gammastep` (night light), `grim` + `slurp` (screenshots).

### Data Flow

```
ocws-daemon.sh
  (inotifywait / pactl subscribe / playerctl -F)
        |
        v
ocws-emit.sh
  (namespace -> sfwbar variable mapping)
        |
        v
sfwbar -R "SetVal XVar = value"
        |
        v
Widget reads XVar in value expression
        |
        v
Label / Image widget renders output
```

## Codebase Structure

```
labwc-fuzzel-sfwbar/
|-- build.zig                     Zig build system for C utilities
|-- install.sh                    Main installer
|-- install-distribution.sh       Distro-specific installer (Arch/Debian/Fedora)
|-- build-ocws-core.sh            Build labwc/sfwbar/fuzzel from source
|-- TODOS.md                      Strategic roadmap and phase tracking
|
|-- src/                          C source files for all utility binaries
|   |-- ocws-sysmon.c             System metrics (CPU/mem/net/bat/bt/brightness/temp)
|   |-- ocws-notify.c             D-Bus notification daemon
|   |-- ocws-osd-notify.c         Glassmorphic notification popup (GTK layer shell)
|   |-- ocws-brightness.c         Smooth backlight control with cubic easing
|   |-- ocws-volume.c             Smooth PulseAudio volume control with cubic easing
|   |-- ocws-wallpaper.c          Time-of-day wallpaper transitions
|   |-- ocws-live-bg.c            Animated live background (GTK layer shell + cairo)
|   |-- ocws-color.c              Wallpaper palette extraction (median-cut)
|   |-- ocws-clip.c               Clipboard manager (cliphist + fuzzel picker)
|   |-- ocws-shot.c               Screenshot tool (grim + slurp + annotation)
|   |-- ocws-ocr.c                Screen OCR via Tesseract
|   |-- ocws-recorder.c           Screen recording (wf-recorder wrapper)
|   |-- ocws-lock.c               Screen lock wrapper (swaylock)
|   |-- ocws-kv.c + ocws-kv-cli.c Key-value persistent store
|   |-- ocws-hypertile.c          Dynamic tiling layout for labwc
|   `-- ocws-style.c              Style helper
|
|-- dotfiles/
|   |-- ocws/                     Core shell configuration and widgets
|   |   |-- ocws.config           Main dual-bar sfwbar config (top + bottom)
|   |   |-- user.config           User overlay (not overwritten by installer)
|   |   |-- plugins.config        Auto-generated widget include list
|   |   |-- ocws.css              Generated panel CSS (theme-engine output)
|   |   |-- theme.css             Static structural CSS (layout, widget geometry)
|   |   |-- ocws-daemon.sh        Background event listener (IPC bridge)
|   |   |-- ocws-sysmon.source    System metrics scanner (CPU/mem/bat/temp)
|   |   |-- cpu.source            Per-CPU utilization scanner
|   |   |-- memory.source         Memory breakdown scanner
|   |   |-- battery.source        Battery level/state scanner
|   |   |-- *.widget              36 widget files (see Widget System below)
|   |   |-- plugins/              Drop-in directory for third-party widgets
|   |   `-- widget-sets/          Widget profile sets (standard.set, full.set)
|   |
|   |-- labwc/                    Compositor configuration
|   |   |-- rc.xml                Keybindings and window rules
|   |   |-- menu.xml              Right-click root menu
|   |   |-- autostart             Boot script (launches sfwbar, daemon, services)
|   |   |-- environment           Environment variables (GDK_BACKEND, etc.)
|   |   `-- themerc-override       WM theme overrides
|   |
|   |-- fuzzel/                   Fuzzel launcher config
|   |-- foot/                     Foot terminal config
|   |-- gtk-3.0/                  GTK3 settings
|   |-- gtk-4.0/                  GTK4 settings
|   |-- fontconfig/               Font rendering config
|   |-- mako/                     Notification daemon config (fallback)
|   |-- rofi/                     Rofi config (legacy)
|   `-- qt6ct/                    Qt6 theme config
|
|-- scripts/                      Automation and IPC tools
|   |-- theme-engine.sh           INI profiles -> rendered configs
|   |-- theme.sh                  Theme switching CLI
|   |-- ocws-emit.sh              Event Bus emitter (namespace -> sfwbar variable)
|   |-- ocws-plugin-loader.sh     Dynamic plugin include generator
|   |-- ocws-state.sh             State coordinator for daemon
|   |-- ocws-configure.sh         Configuration helper
|   |-- font-scale.sh             Global font scaling tool
|   |-- install-fonts.sh          Font installer
|   |-- keybinds.sh               Keybinding manager
|   |-- backup.sh / restore.sh    Config backup/restore
|   |-- dotfiles-sync.sh          Sync dotfiles to installed locations
|   |-- debug-labwc.sh            Labwc debug launcher
|   `-- actions/                  Action scripts for keybindings and widgets
|       |-- audio.sh              Volume control + emit
|       |-- brightness.sh         Backlight control + emit
|       |-- screenshot.sh         grim + slurp capture
|       |-- clipboard.sh          cliphist + fuzzel picker
|       |-- launcher.sh           Fuzzel app launcher
|       |-- power-menu.sh         Power menu (shutdown/reboot/suspend/lock)
|       |-- workspace.sh          Workspace switching
|       |-- network.sh            WiFi/network control
|       |-- window.sh             Window snap/maximize/float
|       |-- fuzzel-emoji.sh       Emoji picker
|       |-- fuzzel-calc.sh        Calculator (bc)
|       `-- ...                   (18 action scripts total)
|
|-- themes/                       INI-based color palette profiles
|   |-- catppuccin-mocha.ini
|   |-- tokyo-night.ini
|   |-- dracula.ini
|   |-- nord.ini
|   |-- rose-pine.ini
|   |-- gruvbox.ini
|   |-- everforest.ini
|   |-- kanagawa.ini
|   |-- one-dark.ini
|   |-- solarized-dark.ini
|   `-- flexoki.ini
|
|-- templates/                    Config templates with {{VAR}} placeholders
|   |-- sfwbar.css.tmpl           Panel CSS template
|   |-- ocws.css.tmpl             OCWS CSS template
|   |-- fuzzel.ini.tmpl           Fuzzel config template
|   |-- foot.ini.tmpl             Foot terminal template
|   |-- environment.tmpl          Environment variables template
|   |-- gtk3-settings.ini.tmpl    GTK3 settings template
|   |-- gtk4-settings.ini.tmpl    GTK4 settings template
|   |-- mako.ini.tmpl             Notification config template
|   |-- rofi.rasi.tmpl            Rofi config template
|   |-- qt6ct.conf.tmpl           Qt6 theme template
|   |-- gtk.css.tmpl              GTK CSS template
|   `-- themerc-override.tmpl     WM theme override template
|
|-- contracts/                    IPC contracts
|   `-- variables.ini             Variable name source of truth
|
|-- docs/                         Documentation
|   |-- getting-started.md        Installation and first-run guide
|   |-- configuration.md          Event Bus API, plugins, CSS, window rules
|   |-- events.md                 Full IPC event contract
|   `-- lessons/                  55 lesson files (sfwbar internals, bugs, patterns)
|
|-- tests/                        Test scripts
|-- sources-learn/                Reference documentation for dependencies
`-- distro/                       Distro-specific installer scripts
    |-- arch.sh
    |-- debian.sh
    `-- fedora.sh
```

## C Utility Binaries

All C utilities are merged into a single `ocws` binary built via `zig build` and installed to `~/.local/bin/`.

| Binary Command | Purpose |
|--------|---------|
| `ocws sysmon` | System metrics in one pass (CPU/mem/net/bat/bt/brightness/temp) |
| `ocws notify` | Native D-Bus notification daemon (replaces mako) |
| `ocws osd-notify` | Glassmorphic notification popup |
| `ocws brightness` | Smooth backlight control with cubic easing |
| `ocws volume` | Smooth PulseAudio volume control with cubic easing |
| `ocws wallpaper` | Time-of-day wallpaper transitions |
| `ocws live-bg` | Animated live background |
| `ocws color` | Wallpaper palette extraction |
| `ocws clip` | Clipboard manager |
| `ocws shot` | Screenshot tool |
| `ocws ocr` | Screen OCR via Tesseract |
| `ocws recorder` | Screen recording |
| `ocws lock` | Screen lock wrapper |
| `ocws kv` | Key-value persistent store |
| `ocws hypertile` | Dynamic tiling layout for labwc |

## Widget System

OCWS ships 36 widget files in `dotfiles/ocws/`. Each widget is a self-contained `.widget` file using sfwbar's `#Api2` syntax.

### Widget Categories

**Core UI**
- `launcher.widget` -- App launcher button (opens fuzzel)
- `workspaces.widget` -- Pager-based workspace switcher
- `clock.widget` -- Time display with calendar popup
- `tray.widget` -- System tray icons
- `showdesktop.widget` -- Show desktop toggle
- `dock.widget` -- macOS-style dock with magnification
- `dock-apps.widget` -- Pinned application launcher dock
- `keybinds.widget` -- Keyboard shortcut reference

**System Metrics (text-style)**
- `cpu-text.widget` -- CPU utilization with detail popup
- `memory-text.widget` -- RAM usage with breakdown popup
- `network-bandwidth.widget` -- Network traffic with detail popup
- `volume-text.widget` -- Volume level with slider popup
- `brightness-text.widget` -- Backlight brightness with slider popup
- `battery-text.widget` -- Battery level with detail popup
- `temperature.widget` -- CPU thermal reading

**Media & Connectivity**
- `media-player.widget` -- Now-playing display (MPRIS/playerctl)
- `media.widget` -- Compact media controls (prev/play/next)
- `bluetooth.widget` -- Bluetooth device status
- `wifi.widget` / `wifi-secret.widget` -- WiFi connection management

**System Control**
- `ocws-control-center.widget` -- Unified popup (volume, brightness, battery, WiFi, BT, media)
- `session.widget` -- Lock, logout, reboot, shutdown
- `clipboard.widget` -- Clipboard history (cliphist + fuzzel)
- `quick-settings.widget` -- One-click toggles
- `power-profile.widget` -- Performance/balanced/saver modes
- `keyboard-layout.widget` -- Current keyboard layout indicator
- `nightlight.widget` -- Blue light filter toggle

**Desktop Widgets (Floating)**
- `desktop-clock.widget` -- Large floating clock
- `desktop-weather.widget` -- Desktop weather
- `desktop-sysmon.widget` -- Desktop hardware monitor

**System Monitoring**
- `sysmon.widget` -- Uptime, load, processes
- `cpu-monitor.widget` -- Detailed CPU stats
- `memory-monitor.widget` -- Detailed memory breakdown
- `disk.widget` -- Disk usage and I/O
- `notification-center.widget` -- Notification history

**Other**
- `weather.widget` -- Current weather conditions (Open-Meteo API)
- `idle-inhibit.widget` -- Prevent screen blank
- `privacy.widget` -- Mic/camera usage indicators
- `custom-script.widget` -- Template for custom widgets

### Widget-Set Profiles

Widgets are organized into profile sets in `widget-sets/`:

- `full.set` -- All widgets (default)
- `standard.set` -- Core widgets only (minimal)

The main config selects a set: `include("widget-sets/full.set")`

### Data Sources

Scanner blocks in `.source` files provide data to widgets:

| Source File | Variables | Consumers |
|-------------|-----------|-----------|
| `ocws-sysmon.source` | `XBatLvl`, `XBatStat`, `XMemPct`, `XNetState`, `XBtState` | battery, memory, network, bluetooth widgets |
| `cpu.source` | `XCpuLoad`, `XCpuUtilization` | cpu-text, cpu-monitor widgets |
| `memory.source` | `XMemTotal`, `XMemUsed`, `XMemBuffers` | memory-monitor widget |
| `battery.source` | `Level`, `Discharging` | (unused -- ocws-sysmon.source supersedes) |

## Event Bus (IPC)

Background processes communicate with the sfwbar UI via `ocws-emit.sh`. This is the sanctioned way to push state updates into the shell.

```bash
ocws-emit.sh <Namespace.Key> <Value>
```

### Event Namespace Reference

| Event Name | sfwbar Variable | Source | Consumers |
|------------|-----------------|--------|-----------|
| `System.Volume` | `XVolLevel` | daemon (pactl subscribe) | volume-text, control-center |
| `System.VolumeMuted` | `XVolMuted` | daemon (pactl subscribe) | volume-text, control-center |
| `System.Brightness` | `XBrightness` | daemon (inotifywait) | brightness-text, control-center |
| `System.Battery` | `XBatLvl` | ocws-sysmon.source | battery-text, control-center |
| `System.BatteryState` | `XBatStat` | ocws-sysmon.source | battery-text |
| `System.Cpu` | `XCpuLoad` | ocws-sysmon.source | cpu-text |
| `System.Memory` | `XMemPct` | ocws-sysmon.source | memory-text |
| `System.Disk` | `XDiskPct` | disk.widget scanner | disk.widget |
| `System.DND` | `XDndState` | (not yet implemented) | (none) |
| `Network.WiFi` | `XNetState` | ocws-sysmon.source | control-center, network-bandwidth |
| `Network.Bluetooth` | `XBtState` | ocws-sysmon.source | bluetooth, control-center |
| `Media.Title` | `XMediaTitle` | media-player.widget scanner | media-player |
| `Media.Artist` | `XMediaArtist` | media-player.widget scanner | media-player |
| `Media.Status` | `XMediaStatus` | media-player.widget scanner | media-player |

Variable names are defined in `contracts/variables.ini` as the single source of truth.

## Theme Engine

OCWS uses an INI-based theme system. Color palettes in `themes/*.ini` are expanded by `scripts/theme-engine.sh` into rendered config files via `templates/*.tmpl`.

### Theme Profiles

11 built-in themes: `catppuccin-mocha`, `tokyo-night`, `dracula`, `nord`, `rose-pine`, `gruvbox`, `everforest`, `kanagawa`, `one-dark`, `solarized-dark`, `flexoki`

### INI Profile Sections

Each theme `.ini` file contains sections for every config surface:

```
[meta]       name, author, description
[colors]     palette (bg, fg, accent, surface, etc.)
[labwc]      window manager theme colors
[gtk3]       GTK3 settings (theme, icons, cursor, fonts)
[gtk4]       GTK4 settings
[fonts]      Font family and size
[sfwbar]     Panel CSS colors
[rofi]       Rofi launcher colors
[mako]       Notification colors
[foot]       Terminal colors
[qt6ct]      Qt6 theme colors
[cursor]     Cursor theme
```

### Usage

```bash
# List available themes
theme-engine.sh list

# Apply a theme (regenerates all config surfaces)
theme-engine.sh apply themes/catppuccin-mocha.ini

# Preview without committing (Ctrl+C to revert)
theme-engine.sh preview themes/tokyo-night.ini
```

### Generated Config Surfaces

The theme engine produces 11 output files from templates:

1. `~/.config/ocws/ocws.css` (from `templates/ocws.css.tmpl`)
2. `~/.config/ocws/theme.css` (from `templates/sfwbar.css.tmpl`)
3. `~/.config/fuzzel/fuzzel.ini` (from `templates/fuzzel.ini.tmpl`)
4. `~/.config/foot/foot.ini` (from `templates/foot.ini.tmpl`)
5. `~/.config/labwc/environment` (from `templates/environment.tmpl`)
6. `~/.config/labwc/themerc-override` (from `templates/themerc-override.tmpl`)
7. `~/.config/gtk-3.0/settings.ini` (from `templates/gtk3-settings.ini.tmpl`)
8. `~/.config/gtk-4.0/settings.ini` (from `templates/gtk4-settings.ini.tmpl`)
9. `~/.config/mako/config` (from `templates/mako.ini.tmpl`)
10. `~/.config/qt6ct/qt6ct.conf` (from `templates/qt6ct.conf.tmpl`)
11. `~/.config/gtk-3.0/gtk.css` (from `templates/gtk.css.tmpl`)

## Build System

OCWS uses Zig as its build system for C utilities. The `build.zig` file compiles all `src/*.c` files into binaries installed to `zig-out/bin/`.

```bash
# Build all C utilities
zig build

# Output in zig-out/bin/
ls zig-out/bin/
```

### Build Dependencies

| Utility | Required Libraries |
|---------|-------------------|
| `ocws-sysmon`, `ocws-clip`, `ocws-shot`, `ocws-lock`, `ocws-brightness`, `ocws-volume`, `ocws-recorder` | libc only |
| `ocws-kv` | libc only |
| `ocws-color`, `ocws-wallpaper` | cairo, libm |
| `ocws-ocr` | tesseract, leptonica |
| `ocws-notify` | glib-2.0, gio-2.0, gobject-2.0 |
| `ocws-osd-notify` | gtk+-3.0, gtk-layer-shell-0, glib-2.0, gio-2.0 |
| `ocws-live-bg` | gtk+-3.0, gtk-layer-shell-0, libm |
| `ocws-hypertile` | wayland-client |

## Installation

### Prerequisites

Core packages (Arch Linux):

```bash
sudo pacman -S labwc sfwbar fuzzel gtk-layer-shell pipewire wireplumber libpulse \
  brightnessctl inotify-tools playerctl bc wl-clipboard cliphist \
  polkit-gnome swayidle swaylock grim slurp foot
```

For Ubuntu/Debian and Fedora, see `distro/debian.sh` and `distro/fedora.sh`.

### Quick Install

```bash
git clone <this-repo> && cd labwc-fuzzel-sfwbar
./install.sh
```

The installer:

1. Checks for required dependencies
2. Backs up existing `~/.config/labwc/` and `~/.config/ocws/`
3. Deploys `dotfiles/labwc/` to `~/.config/labwc/`
4. Deploys `dotfiles/ocws/` to `~/.config/ocws/`
5. Deploys `dotfiles/fuzzel/` to `~/.config/fuzzel/`
6. Deploys GTK settings to `~/.config/gtk-3.0/` and `~/.config/gtk-4.0/`
7. Links all scripts from `scripts/` to `~/.local/bin/`
8. Links action scripts from `scripts/actions/` to `~/.local/bin/actions/`
9. Installs built C binaries from `zig-out/bin/` to `~/.local/bin/`

### Build from Source

To compile the latest upstream versions of labwc, sfwbar, and fuzzel:

```bash
./build-ocws-core.sh all
```

### Installed File Layout

| Path | Contents |
|------|----------|
| `~/.config/labwc/` | `rc.xml`, `menu.xml`, `autostart`, `environment`, `themerc-override` |
| `~/.config/ocws/` | `ocws.config`, `*.widget`, `ocws-daemon.sh`, `plugins/`, `state.kv` |
| `~/.config/noctalia/` | `config.toml` (if using noctalia mode) |
| `~/.config/crystal-dock/` | `panel_1.conf`, `appearance.conf` (if using crystal mode) |
| `~/.config/fuzzel/` | `fuzzel.ini` |
| `~/.config/foot/` | `foot.ini` |
| `~/.local/bin/` | All `scripts/*.sh` and C helper binaries (`ocws-*`) |
| `~/.local/bin/actions/` | All `scripts/actions/*.sh` |

## Keybindings

Defined in `~/.config/labwc/rc.xml`:

| Key | Action |
|-----|--------|
| `Super+Enter` | Launch terminal (foot) |
| `Super+D` | Launch app launcher (fuzzel) |
| `Super+V` | Open clipboard history (cliphist + fuzzel) |
| `Super+Q` | Close focused window |
| `Super+F` | Toggle fullscreen |
| `Super+1-9` | Switch to workspace 1-9 |
| `Super+Shift+1-9` | Move window to workspace 1-9 |
| `Alt+Tab` | Cycle through windows |
| `XF86AudioRaiseVolume` | Volume up |
| `XF86AudioLowerVolume` | Volume down |
| `XF86AudioMute` | Toggle mute |
| `XF86MonBrightnessUp` | Brightness up |
| `XF86MonBrightnessDown` | Brightness down |
| `Print` | Screenshot region to file |
| `Shift+Print` | Screenshot region to clipboard |
| `Super+Print` | Screenshot fullscreen to file |

## Documentation

| File | Description |
|------|-------------|
| `docs/getting-started.md` | Installation, first-run, keybindings, troubleshooting |
| `docs/configuration.md` | Event Bus API, plugin system, CSS theming, window rules, C binaries, action scripts |
| `docs/events.md` | Full IPC event contract with variable mappings and data flow |
| `docs/lessons/` | 55 lesson files covering sfwbar internals, bug patterns, and implementation notes |
| `TODOS.md` | Strategic roadmap (7 phases) with status tracking |
| `dotfiles/ocws/README.md` | Widget inventory with categories and data sources |
