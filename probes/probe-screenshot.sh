#!/bin/bash
# probe-screenshot.sh — Test: wire screenshot to existing ocws-shot C binary
# Goal: Verify ocws-shot covers all modes, identify gaps

set -euo pipefail

OCWS_SHOT="${HOME}/.local/bin/ocws-shot"

echo "=== Screenshot C Rewrite Probe ==="
echo ""

# Check if C binary exists
if [[ ! -x "$OCWS_SHOT" ]]; then
    echo "FAIL: ocws-shot not found at $OCWS_SHOT"
    echo "Build with: zig build"
    exit 1
fi

echo "PASS: ocws-shot binary found"
echo ""

# Test each mode
MODES=("area" "full" "window" "screen")
for mode in "${MODES[@]}"; do
    echo -n "  Testing --mode $mode: "
    if "$OCWS_SHOT" --help 2>&1 | grep -q "$mode"; then
        echo "SUPPORTED"
    else
        echo "MISSING"
    fi
done

echo ""
echo "=== Gap Analysis ==="
echo "Current bash screenshot.sh modes:"
grep -oP '(area|full|window|screen|annotate|timer)' "${HOME}/.config/ocws/scripts/actions/screenshot.sh" 2>/dev/null | sort -u || echo "  (script not found)"
echo ""
echo "C binary modes:"
"$OCWS_SHOT" --help 2>&1 | grep -oP '(area|full|window|screen|annotate|timer)' | sort -u || echo "  (no help output)"
echo ""
echo "=== Recommendation ==="
echo "If C binary covers all bash modes → wire bash to C (like audio.sh does)"
echo "If C binary has gaps → add missing modes to C, then wire"
