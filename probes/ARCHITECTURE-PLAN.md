# OCWS C Rewrite Architecture Plan

## Overview

This document maps bash scripts to C modules, identifying a unified modular architecture
that eliminates duplication and reduces process-forking overhead.

## Architecture: Modular C Toolkit

```
src/
├── libocws/                    # Shared library (already exists)
│   ├── easing.h                # Animation functions
│   ├── sysfs.h                 # Sysfs I/O helpers
│   ├── audio.h                 # PulseAudio helpers
│   ├── ipc.h                   # IPC helpers (emit to sfwbar)
│   ├── http.h                  # HTTP client (libcurl wrapper)
│   ├── config.h                # INI/config parser
│   └── cli.h                   # CLI argument parsing helpers
│
├── cli/                        # CLI binaries (already exists)
│   ├── ocws-volume.c           # DONE — audio control
│   ├── ocws-brightness.c       # DONE — backlight control
│   ├── ocws-clip.c             # DONE — clipboard
│   ├── ocws-shot.c             # DONE — screenshot
│   ├── ocws-kv.c               # DONE — key-value store
│   ├── ocws-emit.c             # DONE — IPC emit
│   ├── ocws-color.c            # DONE — color extraction
│   ├── ocws-sysmon.c           # DONE — system monitor
│   ├── ocws-player.c           # DONE — media player
│   ├── ocws-search.c           # DONE — search
│   ├── ocws-display.c          # NEW — wlr-randr wrapper
│   ├── ocws-theme.c            # NEW — theme engine
│   ├── ocws-font-scale.c       # NEW — font scaling
│   └── ocws-dnd.c              # NEW — Do Not Disturb
│
├── daemons/                    # Long-running daemons
│   ├── ocws-brokerd.c          # DONE — event bus daemon
│   ├── ocws-notify.c           # DONE — notification daemon
│   ├── ocws-osd-notify.c       # DONE — OSD popup
│   ├── ocws-wallpaper.c        # DONE — wallpaper transitions
│   ├── ocws-live-bg.c          # DONE — live background
│   ├── ocws-hypertile.c        # DONE — tiling manager
│   └── ocws-appletd.c          # NEW — applet host (pomodoro, github, crypto)
│
└── gui/                        # GTK3 GUIs (already exists)
    ├── ocws-settings.c         # DONE — settings panel
    ├── ocws-theme-center.c     # DONE — theme center
    ├── ocws-fonts-mgr/         # DONE — fonts manager (modular)
    └── ocws-pkgmgr.c           # DONE — package manager
```

## Script → C Mapping

### Tier 1: Wire to Existing C Binary (Immediate)

| Bash Script | C Binary | Action | Lines Saved |
|-------------|----------|--------|-------------|
| `actions/screenshot.sh` | `ocws-shot` | Wire fallback pattern | 135 |
| `actions/clipboard.sh` | `ocws-clip` | Wire fallback pattern | 107 |
| `actions/mic.sh` | `ocws-volume` | Replace with alias | 86 |
| `actions/kvstore.sh` | `ocws-kv` | Wire fallback pattern | 205 |
| `actions/kvstore-cli.sh` | `ocws-kv` | Wire fallback pattern | 178 |
| `ocws-display.sh` | (duplicate) | Remove, use actions/display.sh | 97 |
| `screenshot-tool.sh` | (duplicate) | Remove, use actions/screenshot.sh | 157 |

**Total: 965 lines eliminated by deduplication + wiring**

### Tier 2: New C Binary (Medium Effort)

| Script | New C Binary | Complexity | Impact |
|--------|-------------|------------|--------|
| `actions/display.sh` | `ocws-display.c` | MEDIUM | MEDIUM |
| `actions/dnd.sh` | `ocws-dnd.c` | LOW | MEDIUM |
| `font-scale.sh` | `ocws-font-scale.c` | MEDIUM | HIGH |

### Tier 3: High-Complexity C Rewrite (Dedicated Session)

| Script | C Module | Complexity | Impact |
|--------|----------|------------|--------|
| `theme-engine.sh` | `ocws-theme.c` | HIGH | HIGH |
| `scripts/applets/*.sh` | `ocws-appletd.c` | MEDIUM | MEDIUM |

### Tier 4: Stay as Bash

| Category | Examples | Reason |
|----------|----------|--------|
| UI menus | rofi/fuzzel launchers | No performance need |
| File ops | backup, restore, sync | Shell is appropriate |
| Installers | package installation | One-shot execution |
| Process mgmt | toggle-shell, start-labwc | Shell is appropriate |

## Fallback Pattern (for Tier 1 scripts)

```bash
#!/bin/bash
# Standard C-first pattern for action scripts

OCWS_BIN="${HOME}/.local/bin/ocws-SUBCOMMAND"

# Try C binary first
if [[ -x "$OCWS_BIN" ]]; then
    exec "$OCWS_BIN" "$@"
fi

# Bash fallback
echo "Warning: ocws-SUBCOMMAND not found, using bash fallback"
# ... original bash logic ...
```

## Applet Module Interface (for Tier 3)

```c
// src/daemons/ocws-appletd.c — applet module system

typedef struct {
    const char *name;
    const char *description;
    int interval_seconds;       // 0 = event-driven

    void (*init)(void *config);
    void (*tick)(void);         // called every interval_seconds
    void (*destroy)(void);
} OCWSApplet;

// Built-in applets
extern OCWSApplet applet_pomodoro;
extern OCWSApplet applet_github;
extern OCWSApplet applet_crypto;

// Registry
static OCWSApplet *applets[] = {
    &applet_pomodoro,
    &applet_github,
    &applet_crypto,
    NULL
};
```

## Build Integration

All new C binaries follow the existing build.zig pattern:

```zig
// New CLI binary
{
    const exe = b.addExecutable(.{
        .name = "ocws-DISPLAY",
        .root_module = b.createModule(.{
            .target = target, .optimize = optimize, .link_libc = true,
        }),
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("src/cli/ocws-DISPLAY.c"),
        .flags = c_flags,
    });
    exe.root_module.linkSystemLibrary("glib-2.0", .{});
    b.installArtifact(exe);
}
```

## Implementation Order

1. **Phase 1 (This session)**: Wire Tier 1 scripts to existing C binaries
2. **Phase 2**: Create `ocws-display.c` and `ocws-dnd.c`
3. **Phase 3**: Create `ocws-font-scale.c`
4. **Phase 4**: Rewrite `theme-engine.sh` → `ocws-theme.c`
5. **Phase 5**: Create `ocws-appletd.c` with applet modules

## Metrics

| Metric | Before | After (Projected) |
|--------|--------|-------------------|
| Bash scripts | 75+ | 50+ (deduplication) |
| Duplicated logic | 965 lines | 0 lines |
| Process forks per event | 3-5 | 1 (C binary) |
| Theme apply time | ~500ms (bash) | ~50ms (C, projected) |
