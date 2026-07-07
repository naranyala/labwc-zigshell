# Modular Configuration System

OCWS uses a composable configuration architecture for SFWBar modes.

---

## Overview

Instead of monolithic config files, OCWS splits configuration into reusable modules:

```
dotfiles/ocws/modes/
├── base.config              # Common settings
├── topbar.config            # Top status bar
├── bottombar.config         # Bottom bar + dock + taskbar
├── statusbar.config         # Single status bar
├── desktop.config           # Floating desktop widgets
├── doublepanel.mode         # Composed: top + bottom + desktop
├── crystaldock.mode         # Composed: statusbar only
├── minimal.mode             # Composed: minimal bar
├── css-glassmorphism.config # Glassmorphism tokens
├── css-bars.config          # Bar panel styles
├── css-widgets.config       # Widget button styles
├── css-taskbar.config       # Taskbar styles
├── css-dock.config          # Dock icon styles
└── css-popups.config        # Popup menu styles
```

---

## Mode Files

### doublepanel.mode (Default)

Dual-panel layout with status bar on top and dock/taskbar on bottom.

```ini
#Api2

switcher { disable = false }

include("modes/base.config")
include("modes/topbar.config")
include("modes/bottombar.config")
include("modes/desktop.config")

include("modes/css-glassmorphism.config")
include("modes/css-bars.config")
include("modes/css-widgets.config")
include("modes/css-taskbar.config")
include("modes/css-dock.config")
include("modes/css-popups.config")
```

**Layout:**
```
┌─────────────────────────────────────────┐
│ Top Bar: Launcher | Workspaces | ...    │
├─────────────────────────────────────────┤
│                                         │
│              Desktop Area               │
│                                         │
├─────────────────────────────────────────┤
│ Bottom: Dock Apps | Taskbar | Clock     │
└─────────────────────────────────────────┘
```

### crystaldock.mode

Single status bar optimized for use with external crystal-dock.

```ini
#Api2

switcher { disable = true }

include("modes/base.config")
include("modes/statusbar.config")

include("modes/css-glassmorphism.config")
include("modes/css-bars.config")
include("modes/css-widgets.config")
include("modes/css-popups.config")
```

**Layout:**
```
┌─────────────────────────────────────────┐
│ Status Bar: Launcher | Clock | Status   │
└─────────────────────────────────────────┘
        (crystal-dock runs separately)
```

### minimal.mode

Minimal status bar with only essential widgets.

```ini
#Api2

switcher { disable = true }

include("modes/base.config")

bar {
  edge = "top"
  layer = "top"
  mirror = "*"
  exclusive_zone = "auto"
  size = 28

  widget "launcher"
  label { css = "* { -GtkWidget-hexpand: true; }" }
  widget "clock"
  label { css = "* { -GtkWidget-hexpand: true; }" }
  widget "volume-text"
  widget "battery-text"
  include("tray.widget")
}

include("modes/css-glassmorphism.config")
include("modes/css-bars.config")
include("modes/css-widgets.config")
include("modes/css-popups.config")
```

**Layout:**
```
┌─────────────────────────────────────────┐
│ Mini Bar: Launcher | Clock | Vol | Bat  │
└─────────────────────────────────────────┘
```

---

## Config Modules

### base.config

Common settings included by all modes:

```ini
#Api2

Set ImagePath = "icons/misc:icons/weather"
Set ThicknessHint = "36px"

Function SfwbarInit() {}

include("plugins.config")
include("ocws-sysmon.source")

include("user.config")
include("theme.css")
include("tokens.css")
```

### topbar.config

Top status bar definition:

```ini
#Api2

bar "topbar:top" {
  edge = "top"
  layer = "top"
  mirror = "*"
  exclusive_zone = "auto"
  size = 32

  widget "launcher"
  widget "workspaces"
  label { css = "* { -GtkWidget-hexpand: true; }" }
  include("widget-sets/full.set")
}
```

### bottombar.config

Bottom bar with dock and taskbar:

```ini
#Api2

bar "bottombar:bottom" {
  edge = "bottom"
  layer = "top"
  mirror = "*"
  exclusive_zone = "auto"
  size = 40

  widget "dock"
  taskbar { ... }
  widget "showdesktop"
  include("tray.widget")
  widget "clock"
}
```

---

## CSS Modules

### css-glassmorphism.config

Defines glassmorphism tokens and base styles:

```css
@define-color shell_bg alpha(@ocws_bg, 0.92);
@define-color shell_border alpha(@ocws_fg, 0.1);
@define-color text_primary @ocws_fg;
@define-color shadow rgba(0, 0, 0, 0.4);

* {
  background-color: transparent;
  color: @text_primary;
  font-family: 'FiraCode Nerd Font', 'Noto Sans', sans-serif;
}
```

### css-widgets.config

Widget button and pill styles:

```css
button.module {
  padding: 0px 10px;
  border-radius: 18px;
  background-color: rgba(49, 50, 68, 0.85);
  border: 1px solid @shell_border;
  transition: all 0.15s ease-out;
}

button.module:hover {
  background-color: rgba(69, 71, 90, 0.9);
}
```

---

## Switching Modes

### Using toggle-shell

```bash
toggle-shell doublepanel
toggle-shell crystaldock
toggle-shell minimal
```

### Using sfwbar-mode

```bash
sfwbar-mode start doublepanel
sfwbar-mode start crystaldock
sfwbar-mode start minimal
sfwbar-mode status
sfwbar-mode validate
```

### Manual

```bash
sfwbar -c ~/.config/ocws/modes/doublepanel.mode
```

---

## Exposed Settings

`settings.config` provides user-configurable options:

```ini
# Bar sizes
Set OCWS_TOP_BAR_SIZE = "32"
Set OCWS_BOTTOM_BAR_SIZE = "40"

# Visual settings
Set OCWS_BG_ALPHA = "0.92"
Set OCWS_TRANSITION_DURATION = "0.15s"

# Feature toggles
Set OCWS_FEATURE_DOCK = "true"
Set OCWS_FEATURE_TASKBAR = "true"
```

---

## Validation

```bash
# Validate all configs
scripts/validate-sfwbar.sh

# Validate modes
sfwbar-mode validate
```

Checks:
- `#Api2` header presence
- Brace and quote matching
- Include file references
- Duplicate bar names
- Widget references
- CSS token usage
