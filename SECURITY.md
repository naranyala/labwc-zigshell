# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability in OCWS, please report it responsibly.

**Do NOT open a public GitHub issue for security vulnerabilities.**

### How to Report

1. Email: [security@ocws.dev] (or your preferred contact)
2. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

### What to Expect

- Acknowledgment within 48 hours
- Assessment within 1 week
- Fix timeline communicated after assessment
- Credit in release notes (unless you prefer anonymity)

## Security Measures

### Build-time

- `-Wall -Wextra -Werror` compiler flags
- AddressSanitizer in CI (debug builds)
- cppcheck static analysis in CI
- musl static linking (reduces attack surface)

### Runtime

- `umask(0077)` on all daemon startup (private file permissions)
- `$XDG_RUNTIME_DIR` for PID files and temp data (not `/tmp`)
- `getpwuid()` fallback instead of `/tmp` when `$HOME` is unset
- Shell metacharacter validation before any `system()` or `execl()` call
- Path traversal rejection (`../`, `/`, `\`) on user-supplied names
- `volatile sig_atomic_t` for signal handlers (async-signal-safe)
- API version checking on plugin loading

### Known Limitations

- Plugin loading via `dlopen()` does not verify signatures/checksums
  (future: checksum or signature verification for plugin .so files)
- D-Bus methods have no access control beyond session bus isolation

## Hardening Roadmap

- [ ] Plugin signature verification (checksum or ed25519)
- [ ] D-Bus method access control (polkit or peer credential check)
- [ ] Fuzzing harness for INI parser and theme engine
- [ ] `O_CLOEXEC` on all pipe/FD creation
