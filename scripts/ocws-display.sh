#!/bin/bash
# ocws-display.sh — Thin wrapper around actions/display.sh
# Delegates all operations to the main display script.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/actions/display.sh" "$@"
