#!/bin/bash
# probe-theme-engine.sh — Test: theme-engine.sh bottlenecks
# Goal: Profile the bash theme engine to identify C rewrite hotspots

set -euo pipefail

THEME_ENGINE="${HOME}/.config/ocws/scripts/theme-engine.sh"
PROJECT_THEME_ENGINE="/media/naranyala/Data/projects-remote/labwc-fuzzel-sfwbar/scripts/theme-engine.sh"
ENGINE="${PROJECT_THEME_ENGINE:-$THEME_ENGINE}"

echo "=== Theme Engine C Rewrite Probe ==="
echo ""

if [[ ! -f "$ENGINE" ]]; then
    echo "FAIL: theme-engine.sh not found"
    exit 1
fi

echo "Script size: $(wc -l < "$ENGINE") lines"
echo ""

echo "--- Phase 1: INI Parsing ---"
echo "Lines: $(sed -n '/^parse_ini/,/^}/p' "$ENGINE" 2>/dev/null | wc -l)"
echo "Pattern: while read + regex section/key detection"
echo "C equivalent: struct ini_section { char name[64]; struct ini_key *keys; }"
echo ""

echo "--- Phase 2: Variable Expansion ---"
echo "Lines: $(sed -n '/^expand_vars/,/^}/p' "$ENGINE" 2>/dev/null | wc -l)"
echo "Pattern: 5-pass regex replacement loop"
echo "C equivalent: hash table lookup + string replace"
echo ""

echo "--- Phase 3: Template Rendering ---"
echo "Lines: $(sed -n '/^render_template/,/^}/p' "$ENGINE" 2>/dev/null | wc -l)"
echo "Pattern: case statement matching 100+ template variables"
echo "C equivalent: switch statement + file I/O"
echo ""

echo "--- Phase 4: File Writing ---"
echo "Lines: $(sed -n '/^cmd_apply/,/^}/p' "$ENGINE" 2>/dev/null | wc -l)"
echo "Pattern: mkdir + echo to file"
echo "C equivalent: fwrite + mkdir"
echo ""

echo "=== Benchmark ==="
echo "Timing bash theme-engine.sh apply:"
time bash "$ENGINE" apply "${HOME}/.config/ocws/themes/catppuccin-mocha.ini" 2>/dev/null || echo "(no themes installed)"
echo ""

echo "=== C Rewrite Plan ==="
echo "Module 1: ocws-ini.c — INI parser (50 lines)"
echo "Module 2: ocws-template.c — Template renderer (200 lines)"
echo "Module 3: ocws-theme.c — CLI wrapper (50 lines)"
echo "Total: ~300 lines C replacing 648 lines bash"
