#!/bin/bash
#
#
# Creates timestamped backup of all config files.
# Supports incremental (only changed files) and full backup.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="${HOME}/.config/labwc"
BACKUP_BASE="${HOME}/.config/labwc-backups"
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
BACKUP_DIR="${BACKUP_BASE}/${TIMESTAMP}"

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

# Parse args
MODE="full"
KEEP=5  # Keep last N backups

while [[ $# -gt 0 ]]; do
  case $1 in
    --incremental) MODE="incremental"; shift ;;
    --keep) KEEP="$2"; shift 2 ;;
    --dir) BACKUP_DIR="$2"; shift 2 ;;
    --help)
      echo "Usage: $0 [--incremental] [--keep N] [--dir PATH]"
      echo ""
      echo "Options:"
      echo "  --incremental   Only backup changed files"
      echo "  --keep N        Keep last N backups (default: 5)"
      echo "  --dir PATH      Custom backup directory"
      exit 0
      ;;
    *) warn "Unknown option: $1"; shift ;;
  esac
done

echo ""
echo "== labwc Backup =="
echo ""
info "Mode: $MODE"
info "Backup dir: $BACKUP_DIR"
echo ""

# Create backup directory
mkdir -p "$BACKUP_DIR/labwc"
mkdir -p "$BACKUP_DIR/scripts"
mkdir -p "$BACKUP_DIR/dotfiles"

BACKED_UP=0
SKIPPED=0

backup_file() {
  local src="$1"
  local dest_dir="$2"
  local filename=$(basename "$src")
  
  if [ ! -f "$src" ]; then
    return
  fi
  
  if [ "$MODE" = "incremental" ]; then
    # Check if file changed since last backup
    local last_backup=""
      if [ -f "$dir/$filename" ]; then
        last_backup="$dir/$filename"
        break
      fi
    done
    
    if [ -n "$last_backup" ] && [ -f "$last_backup" ]; then
      if cmp -s "$src" "$last_backup" 2>/dev/null; then
        info "  Skip (unchanged): $filename"
        ((SKIPPED++))
        return
      fi
    fi
  fi
  
  cp "$src" "$dest_dir/"
  pass "$filename"
  ((BACKED_UP++))
}

# --- Backup labwc config ---
section "labwc config"
for cfg in rc.xml autostart environment menu.xml themerc-override; do
  backup_file "$CONFIG_DIR/$cfg" "$BACKUP_DIR/labwc"
done

fi

# --- Backup scripts ---
section "scripts"
backup_file "$SCRIPT_DIR/toggle-natural-scroll.sh" "$BACKUP_DIR/scripts"
backup_file "$SCRIPT_DIR/start-redshift.sh" "$BACKUP_DIR/scripts"
backup_file "$SCRIPT_DIR/start-labwc.sh" "$BACKUP_DIR/scripts"

# --- Backup dotfiles ---
section "dotfiles"
backup_file "$SCRIPT_DIR/../dotfiles/wallpaper" "$BACKUP_DIR/dotfiles"
backup_file "$SCRIPT_DIR/../dotfiles/wallpaper-sources.txt" "$BACKUP_DIR/dotfiles"

# --- Create metadata ---
cat > "$BACKUP_DIR/manifest.txt" << EOF
labwc backup
timestamp: $TIMESTAMP
mode: $MODE
hostname: $(hostname)
user: $(whoami)
labwc version: $(labwc --version 2>/dev/null || echo "unknown")
EOF

# --- Compress ---
section "Compressing"
ARCHIVE="${BACKUP_BASE}/${TIMESTAMP}.tar.gz"
tar -czf "$ARCHIVE" -C "$BACKUP_BASE" "$TIMESTAMP" 2>/dev/null
rm -rf "$BACKUP_DIR"
pass "Created: $ARCHIVE"

# --- Cleanup old backups ---
section "Cleanup"
BACKUP_COUNT=$(ls -1 "$BACKUP_BASE"/*.tar.gz 2>/dev/null | wc -l)
if [ "$BACKUP_COUNT" -gt "$KEEP" ]; then
  REMOVE_COUNT=$((BACKUP_COUNT - KEEP))
  ls -1t "$BACKUP_BASE"/*.tar.gz | tail -n "$REMOVE_COUNT" | while read -r old; do
    rm -f "$old"
    pass "Removed old backup: $(basename "$old")"
  done
fi

# --- Summary ---
echo ""
echo -e "${GREEN}${BOLD}Backup complete${NC}"
echo "  Files backed up: $BACKED_UP"
echo "  Files skipped:   $SKIPPED"
echo "  Archive:         $ARCHIVE"
echo "  Backups kept:    $KEEP"
echo ""
echo "Restore with: ./scripts/restore.sh $ARCHIVE"
echo ""
