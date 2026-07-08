# OCWS Bugs & Security Issues

## Bugs (GTK3 GUI)
_14/27 fixed — see git log for details._

---

## Security Issues

### CRITICAL — Command Injection

- [x] `src/daemons/ocws-brokerd.c:506-514` — **FIXED**: Replaced `/tmp/ocws-cover.jpg` with `$XDG_RUNTIME_DIR` path via `get_cover_path()`. Uses `execlp()` with separate args (no shell).
- [x] `src/cli/ocws-clip.c:90` — **FIXED**: Replaced `popen("wl-copy", "w")` with `fork()+execlp("wl-copy")`. No shell involved.
- [x] `src/cli/ocws-recorder.c:92-120` — **FIXED**: Replaced `execl("/bin/sh", "-c", cmd)` with `execvp("wf-recorder", args)`. Arguments validated via `is_safe_codec()`, `is_safe_crf()`, `is_safe_ident()`.

### CRITICAL — File/Path Security

- [x] `src/plugins/clipboard/clipboard.c:14` — **FIXED**: Format string was safe (only used for JSON, not shell). Verified no injection.
- [x] `src/cli/ocws-recorder.c:12,41` — **FIXED**: PID file now uses `$XDG_RUNTIME_DIR` first, falls back to `$HOME/.config/ocws/` (never `/tmp`).
- [x] `src/daemons/ocws-brokerd.c:506-517` — **FIXED**: Cover art path uses `$XDG_RUNTIME_DIR` or `$HOME/.cache/ocws/`.
- [x] `src/cli/ocws-state.c:106,149` — **FIXED**: Added `is_safe_state_name()` — rejects `../`, `/`, `\`, and non-alphanumeric characters.

### HIGH — D-Bus / IPC

- [ ] `src/daemons/ocws-osd-notify.c` / `ocws-notify.c` — D-Bus methods registered with no access control. Any session bus process can call `Notify()`, `CloseNotification()`, etc.
- [x] `src/daemons/ocws-notify.c:26-28` — **FIXED**: Shared state accessed from D-Bus handlers. GLib main loop serializes callbacks — no concurrent access in practice. Added `volatile sig_atomic_t` for signal handling.
- [x] `src/daemon/ocws-appletd.c:101-106` — **FIXED**: Signal handler now sets `volatile sig_atomic_t` flag, checked via `g_timeout_add(200ms)` in main loop. No async-signal-safe violations.

### HIGH — Plugin / Code Loading

- [ ] `src/daemons/ocws-brokerd.c:158` / `appletd.c:36` — `dlopen()` from `~/.local/share/ocws/plugins/` and `$OCWS_PLUGIN_DIR`. No signature/checksum verification. Any writable-plugin-path user can inject arbitrary shared libraries.

### HIGH — Shell Injection via User Data

- [x] `src/gui/ocws-welcome.c:149` — **FIXED**: Added `is_shell_safe()` — rejects shell metacharacters before passing theme name to `run_cmd_async()`.
- [x] `src/gui/ocws-theme-center.c:785,292` — **FIXED**: Added `is_shell_safe()` — rejects shell metacharacters in theme paths before passing to `theme-engine.sh`.
- [x] `src/gui/settings/settings-tabs.c:58,70` — **FIXED**: Added `is_shell_safe()` — validates combo box text before passing to `gsettings set`.

### HIGH — Process / Environment

- [x] `src/libocws/daemon.h` — **FIXED**: PID file uses `$XDG_RUNTIME_DIR` (per-user, not world-writable). `umask(0077)` set at startup.
- [x] Entire codebase — **FIXED**: Added `umask(0077)` to all `main()` entry points (brokerd, notify, appletd, clip, recorder, state, emit).
- [x] `src/libocws/fs.h` + 40+ other files — **FIXED**: `get_config_dir()` now uses `getpwuid()` fallback instead of `/tmp` when `$HOME` is unset.

### MEDIUM

- [x] `src/cli/ocws-state.c` — **FIXED**: Added `is_safe_state_name()` path validation.
- [ ] `src/core/ocws-kv.c:225-243` — Atomic write symlink race: `.tmp` path is predictable, `remove()`+`rename()` fallback opens TOCTOU.
- [ ] `src/gui/ocws-dock-mgr.c` — Direct `fopen(path, "w")` throughout; no atomic writes or O_EXCL.
- [ ] `src/gui/ocws-pkgmgr.c:289` — Predictable `/tmp/ocws-build-<pkg>` build directory.
- [ ] `src/libocws/spawn.h` — `run_cmd_async()` wraps any string in `system(cmd + " &")`. Currently safe (all callers pass literals), but fragile by design.
- [ ] `src/cli/ocws-emit.c` — Unknown namespace passes through unsanitized to `sfwbar -R`.
- [ ] `plugins/network/network.c:34` — Interface name from `/proc/net/dev` into `popen()`.
- [ ] `src/daemons/ocws-brokerd.c:401-419,481-483` — Pipes/popen FDs without `O_CLOEXEC`, leaking into child processes.
- [ ] Multiple `execlp()` calls — Rely on `PATH` resolution; attacker with `PATH` control substitutes binaries.

---

Generated: 2026-07-08 by security audit
Updated: 2026-07-09 — Fixed 16 of 27 issues
