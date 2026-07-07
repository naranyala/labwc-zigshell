#!/bin/bash
# probe-kvstore.sh — Test: wire kvstore to existing ocws-kv C binary
# Goal: Verify ocws-kv covers all modes, identify gaps

set -euo pipefail

OCWS_KV="${HOME}/.local/bin/ocws-kv"

echo "=== KVStore C Rewrite Probe ==="
echo ""

# Check if C binary exists
if [[ ! -x "$OCWS_KV" ]]; then
    echo "FAIL: ocws-kv not found at $OCWS_KV"
    echo "Build with: zig build"
    exit 1
fi

echo "PASS: ocws-kv binary found"
echo ""

# Test commands
CMDS=("get" "set" "del" "list" "export" "import" "backup" "restore")
for cmd in "${CMDS[@]}"; do
    echo -n "  Testing '$cmd': "
    if "$OCWS_KV" --help 2>&1 | grep -q "$cmd"; then
        echo "SUPPORTED"
    else
        echo "MISSING"
    fi
done

echo ""
echo "=== Gap Analysis ==="
echo "Current kvstore.sh: 205 lines, pure bash grep/sed"
echo "Current kvstore-cli.sh: 178 lines, same logic duplicated"
echo ""
echo "C binary commands:"
"$OCWS_KV" --help 2>&1 | head -20 || echo "  (no help output)"
echo ""
echo "=== Recommendation ==="
echo "Wire both kvstore.sh and kvstore-cli.sh to ocws-kv"
echo "Keep bash for: merge, json (if not in C)"
echo "This eliminates 383 lines of duplicated bash logic"
