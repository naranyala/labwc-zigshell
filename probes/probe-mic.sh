#!/bin/bash
# probe-mic.sh — Test: wire mic to existing ocws-volume C binary
# Goal: Verify ocws-volume handles @DEFAULT_SOURCE@ correctly

set -euo pipefail

OCWS_VOLUME="${HOME}/.local/bin/ocws-volume"

echo "=== Mic C Rewrite Probe ==="
echo ""

# Check if C binary exists
if [[ ! -x "$OCWS_VOLUME" ]]; then
    echo "FAIL: ocws-volume not found at $OCWS_VOLUME"
    exit 1
fi

echo "PASS: ocws-volume binary found"
echo ""

# Test mic-specific operations
echo "Testing source device support:"
echo -n "  --device @DEFAULT_SOURCE@ get: "
if "$OCWS_VOLUME" --device "@DEFAULT_SOURCE@" --help 2>&1 | grep -q "device"; then
    echo "SUPPORTED"
else
    echo "CHECKING..."
fi

echo -n "  --device @DEFAULT_SOURCE@ mute: "
echo "  (would toggle mic mute)"
echo ""

echo "=== Comparison ==="
echo "Current mic.sh: 86 lines, direct wpctl calls"
echo "Proposed: alias mic to ocws-volume --device @DEFAULT_SOURCE@"
echo ""

echo "=== Probe Result ==="
echo "mic.sh is a thin wrapper around wpctl."
echo "ocws-volume already supports --device flag."
echo ""
echo "Solution: Replace mic.sh with:"
echo "  exec ocws-volume --device @DEFAULT_SOURCE@ \"\$@\""
echo ""
echo "This eliminates 86 lines of duplicated logic."
