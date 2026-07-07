#!/bin/bash
# probe-display.sh — Test: consolidate display.sh + ocws-display.sh
# Goal: Merge duplicates, plan C binary for wlr-randr wrapper

set -euo pipefail

echo "=== Display C Rewrite Probe ==="
echo ""

DISPLAY_SCRIPT="${HOME}/.config/ocws/scripts/actions/display.sh"
OCWS_DISPLAY="${HOME}/.config/ocws/scripts/ocws-display.sh"
PROJECT_DISPLAY="/media/naranyala/Data/projects-remote/labwc-fuzzel-sfwbar/scripts/actions/display.sh"
PROJECT_OCWS_DISPLAY="/media/naranyala/Data/projects-remote/labwc-fuzzel-sfwbar/scripts/ocws-display.sh"

echo "--- Duplicate Analysis ---"
echo "actions/display.sh: $(wc -l < "${PROJECT_DISPLAY:-/dev/null}" 2>/dev/null || echo 'N/A') lines"
echo "ocws-display.sh: $(wc -l < "${PROJECT_OCWS_DISPLAY:-/dev/null}" 2>/dev/null || echo 'N/A') lines"
echo ""

echo "--- Common Functions ---"
if [[ -f "$PROJECT_DISPLAY" && -f "$PROJECT_OCWS_DISPLAY" ]]; then
    echo "display.sh functions:"
    grep -oP '^\w+\(\)' "$PROJECT_DISPLAY" 2>/dev/null | head -10
    echo ""
    echo "ocws-display.sh functions:"
    grep -oP '^\w+\(\)' "$PROJECT_OCWS_DISPLAY" 2>/dev/null | head -10
    echo ""
    echo "Overlap:"
    comm -12 <(grep -oP '^\w+\(\)' "$PROJECT_DISPLAY" 2>/dev/null | sort) \
             <(grep -oP '^\w+\(\)' "$PROJECT_OCWS_DISPLAY" 2>/dev/null | sort) 2>/dev/null || echo "  (analysis failed)"
fi

echo ""
echo "--- wlr-randr Usage ---"
echo "Commands used:"
grep -h 'wlr-randr' "$PROJECT_DISPLAY" "$PROJECT_OCWS_DISPLAY" 2>/dev/null | sort -u | head -10

echo ""
echo "=== C Binary Plan ==="
echo "ocws-display.c — wlr-randr wrapper"
echo "Commands: list, layout, save, load, single, mirror, reset"
echo "Uses: wlr-randr JSON output + libjson-c"
echo "Estimated: ~200 lines C"
