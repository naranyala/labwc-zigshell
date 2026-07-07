#!/bin/bash
# kvstore-cli.sh — Thin wrapper around kvstore.sh
# Delegates all operations to the main kvstore.sh script.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/kvstore.sh" "$@"
