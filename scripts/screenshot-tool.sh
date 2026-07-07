#!/bin/bash
# screenshot-tool.sh — Thin wrapper around actions/screenshot.sh
# Delegates all operations to the main screenshot script.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/actions/screenshot.sh" "$@"
