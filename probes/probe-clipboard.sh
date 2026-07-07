#!/bin/bash
# probe-clipboard.sh — Test: wire clipboard to existing ocws-clip C binary
# Goal: Verify ocws-clip covers all modes, identify gaps

set -euo pipefail

OCWS_CLIP="${HOME}/.local/bin/ocws-clip"

echo "=== Clipboard C Rewrite Probe ==="
echo ""

# Check if C binary exists
if [[ ! -x "$OCWS_CLIP" ]]; then
    echo "FAIL: ocws-clip not found at $OCWS_CLIP"
    echo "Build with: zig build"
    exit 1
fi

echo "PASS: ocws-clip binary found"
echo ""

# Test commands
CMDS=("pick" "copy" "clear" "list" "delete")
for cmd in "${CMDS[@]}"; do
    echo -n "  Testing '$cmd': "
    if "$OCWS_CLIP" --help 2>&1 | grep -q "$cmd"; then
        echo "SUPPORTED"
    else
        echo "MISSING"
    fi
done

echo ""
echo "=== Gap Analysis ==="
echo "Current bash clipboard.sh commands:"
grep -oP '(pick|copy|clear|list|delete|paste|search)' "${HOME}/.config/ocws/scripts/actions/clipboard.sh" 2>/dev/null | sort -u || echo "  (script not found)"
echo ""
echo "C binary commands:"
"$OCWS_CLIP" --help 2>&1 | grep -oP '(pick|copy|clear|list|delete|paste|search)' | sort -u || echo "  (no help output)"
echo ""
echo "=== Recommendation ==="
echo "Wire clipboard.sh to ocws-clip for all supported modes"
echo "Keep bash fallback for missing modes (paste, search)"
