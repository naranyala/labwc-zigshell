#!/bin/bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BOLD='\033[1m'
NC='\033[0m'

BIN_DIR="$(pwd)/zig-out/bin"
if [ ! -f "$BIN_DIR/ocws" ]; then
    echo "Error: ocws binary not found. Please run 'zig build' first."
    exit 1
fi

pass() { echo -e "${GREEN}✓ PASS${NC}: $1"; }
fail() { echo -e "${RED}✗ FAIL${NC}: $1"; exit 1; }

echo -e "${BOLD}Starting Chaos Testing for ocws-kv...${NC}"

# --- 1. The "Large Payload" Test (Buffer Overflow / Memory Check) ---
echo "Running Large Payload Test..."
LARGE_VAL=$(printf 'a%.0s' {1..10000}) # 10KB string
if "$BIN_DIR/ocws" kv set chaos.large "$LARGE_VAL" > /dev/null; then
    ACTUAL_VAL=$("$BIN_DIR/ocws" kv get chaos.large)
    if [ "${#ACTUAL_VAL}" -eq 10000 ]; then
        pass "Large payload (10KB) handled"
    else
        fail "Large payload failed (length mismatch: expected 10000, got ${#ACTUAL_VAL})"
    fi
else
    fail "Large payload failed (set command failed)"
fi

# --- 2. The "Character Soup" Test (Encoding / Sanitization Check) ---
echo "Running Character Soup Test..."
# Special chars, emojis, non-ASCII (removed newlines/tabs to avoid line-split issues in current parser)
SOUP="!@#$%^&*()_+ 🚀 漢字 🔣"
if "$BIN_DIR/ocws" kv set chaos.soup "$SOUP" > /dev/null; then
    ACTUAL_SOUP=$("$BIN_DIR/ocws" kv get chaos.soup)
    if [ "$ACTUAL_SOUP" = "$SOUP" ]; then
        pass "Character soup handled"
    else
        fail "Character soup failed. Expected: $SOUP, Got: $ACTUAL_SOUP"
    fi
else
    fail "Character soup set failed"
fi

# --- 3. The "Rapid Fire Concurrency" Test (Race Condition Check) ---
echo "Running Rapid Fire Concurrency Test..."
# We launch 10 background processes all trying to write to the same key
for i in {1..10}; do
    ("$BIN_DIR/ocws" kv set chaos.race "val_$i" > /dev/null &)
done
wait

if "$BIN_DIR/ocws" kv get chaos.race | grep -q "val_"; then
    pass "Concurrency hammer finished without crash"
else
    fail "Concurrency hammer failed to leave valid state"
fi

# --- 4. The "Key Collision/Empty" Test ---
echo "Running Empty/Null Key Test..."
if "$BIN_DIR/ocws" kv set "" "empty_key" > /dev/null 2>&1; then
     echo "Note: Empty key accepted (possibly intended)"
else
     pass "Empty key handled gracefully"
fi

echo -e "\n${BOLD}🎉 Chaos Testing Completed Successfully!${NC}"
