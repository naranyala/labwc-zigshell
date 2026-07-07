#!/bin/bash
#
# clean.sh — Clean up labwc build artifacts, temp files, and old backups
#
# Removes: build dirs, stale labwc clones, old backup archives, temp files.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

pass()  { echo -e "  ${GREEN}✓${NC} $1"; }
warn()  { echo -e "  ${YELLOW}⚠${NC} $1"; }
info()  { echo -e "  ${CYAN}→${NC} $1"; }
section() { echo -e "\n${BOLD}[$1]${NC}"; }

CLEANED=0
DRY_RUN=false
AGGRESSIVE=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --dry-run) DRY_RUN=true; shift ;;
    --all|--aggressive) AGGRESSIVE=true; shift ;;
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --dry-run     Show what would be removed without deleting"
      echo "  --help        Show this help"
      echo ""
      exit 0
      ;;
    *) shift ;;
  esac
done

echo ""
echo "== labwc Cleanup =="
echo ""

REMOVE_DIR() {
  local dir="$1"
  local label="${2:-$dir}"
  if [ -d "$dir" ]; then
    if $DRY_RUN; then
      info "Would remove: $label ($dir)"
    else
      rm -rf "$dir"
      pass "Removed: $label"
      ((CLEANED++))
    fi
  fi
}

REMOVE_FILE() {
  local file="$1"
  local label="${2:-$file}"
  if [ -f "$file" ]; then
    if $DRY_RUN; then
      info "Would remove: $label ($file)"
    else
      rm -f "$file"
      pass "Removed: $label"
      ((CLEANED++))
    fi
  fi
}

# --- Build artifacts ---
section "Build Artifacts"
REMOVE_DIR "$PROJECT_DIR/build" "project build directory"
REMOVE_DIR "$PROJECT_DIR/labwc" "cloned labwc source"

# Stale .o files from any dir
if $AGGRESSIVE; then
  while IFS= read -r -d '' f; do
    REMOVE_FILE "$f" "$f"
  done < <(find "$PROJECT_DIR" -name '*.o' -o -name '*.so' -o -name '*.dylib' 2>/dev/null -print0)
fi

# --- Temp files ---
section "Temp Files"
while IFS= read -r -d '' f; do
  REMOVE_FILE "$f" "$f"
done < <(find "$PROJECT_DIR" -name '*.swp' -o -name '*.swo' -o -name '*~' 2>/dev/null -print0)

# --- Old backups ---
section "Backups"
BACKUP_BASE="${HOME}/.config/labwc-backups"
if [ -d "$BACKUP_BASE" ]; then
  if $DRY_RUN; then
    local count=$(ls -1 "$BACKUP_BASE"/*.tar.gz 2>/dev/null | wc -l)
    info "Would keep: $BACKUP_BASE (${count} archive(s))"
    info "  Manage with: $SCRIPT_DIR/backup.sh --keep N"
  else
    info "Keeping backup directory: $BACKUP_BASE"
    info "  Manage backups with: $SCRIPT_DIR/backup.sh --keep N"
  fi
fi

if $AGGRESSIVE; then
  section "Cached Data"
  REMOVE_DIR "${HOME}/.cache/labwc" "labwc cache"
fi

# --- Summary ---
echo ""
if $DRY_RUN; then
  echo -e "${YELLOW}${BOLD}Dry run${NC} — $CLEANED item(s) would be removed"
  echo "  Re-run without --dry-run to apply."
else
  if [ "$CLEANED" -eq 0 ]; then
    pass "Nothing to clean"
  else
    echo -e "${GREEN}${BOLD}$CLEANED item(s) removed${NC}"
  fi
fi
echo ""
