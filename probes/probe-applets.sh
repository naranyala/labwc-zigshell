#!/bin/bash
# probe-applets.sh — Test: applet scripts as C daemon modules
# Goal: Analyze applet loop patterns for C rewrite

set -euo pipefail

echo "=== Applet C Rewrite Probe ==="
echo ""

APPLETS_DIR="${HOME}/.config/ocws/scripts/applets"

echo "--- Pomodoro Timer ---"
if [[ -f "$APPLETS_DIR/pomodoro.sh" ]]; then
    echo "Lines: $(wc -l < "$APPLETS_DIR/pomodoro.sh")"
    echo "Loop pattern:"
    grep -n "while\|sleep\|ocws-emit\|ocws-notify" "$APPLETS_DIR/pomodoro.sh" 2>/dev/null | head -10
    echo ""
    echo "C module interface:"
    echo "  void applet_pomodoro_init(void);"
    echo "  void applet_pomodoro_tick(void);  // called every N seconds"
    echo "  void applet_pomodoro_destroy(void);"
else
    echo "  (not found)"
fi

echo ""
echo "--- GitHub Notifications ---"
if [[ -f "$APPLETS_DIR/github-notifications.sh" ]]; then
    echo "Lines: $(wc -l < "$APPLETS_DIR/github-notifications.sh")"
    echo "Loop pattern:"
    grep -n "while\|sleep\|curl\|ocws-emit\|ocws-notify" "$APPLETS_DIR/github-notifications.sh" 2>/dev/null | head -10
    echo ""
    echo "C module interface:"
    echo "  void applet_github_init(const char *token);"
    echo "  void applet_github_tick(void);"
    echo "  void applet_github_destroy(void);"
else
    echo "  (not found)"
fi

echo ""
echo "--- Crypto Ticker ---"
if [[ -f "$APPLETS_DIR/crypto-ticker.sh" ]]; then
    echo "Lines: $(wc -l < "$APPLETS_DIR/crypto-ticker.sh")"
    echo "Loop pattern:"
    grep -n "while\|sleep\|curl\|ocws-emit" "$APPLETS_DIR/crypto-ticker.sh" 2>/dev/null | head -10
    echo ""
    echo "C module interface:"
    echo "  void applet_crypto_init(const char *coins);"
    echo "  void applet_crypto_tick(void);"
    echo "  void applet_crypto_destroy(void);"
else
    echo "  (not found)"
fi

echo ""
echo "=== Architecture ==="
echo "All applets share the same pattern:"
echo "  1. init() — load config, set up HTTP client"
echo "  2. tick() — fetch data, emit to sfwbar, send notification"
echo "  3. destroy() — cleanup"
echo ""
echo "C implementation in ocws-brokerd:"
echo "  - Register applet modules at startup"
echo "  - Call tick() on configurable interval"
echo "  - Share HTTP client (libcurl) across applets"
echo "  - Share IPC client (ocws-emit) across applets"
