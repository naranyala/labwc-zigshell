#!/bin/bash
#
#
# Restores from timestamped backup archive created by backup.sh.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="${HOME}/.config/labwc"
BACKUP_BASE="${HOME}/.config/labwc-backups"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

pass()  { echo -e "  ${GREEN}✓${NC} $1"; }
warn()  { echo -e "  ${YELLOW}⚠${NC} $1"; }
info()  { echo -e "  ${CYAN}→${NC} $1"; }
fail()  { echo -e "  ${RED}✗${NC} $1"; exit 1; }
section() { echo -e "\n${BOLD}[$1]${NC}"; }

# Parse args
BACKUP_FILE=""
DRY_RUN=false
FORCE=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --dry-run) DRY_RUN=true; shift ;;
    --force) FORCE=true; shift ;;
    --help)
      echo "Usage: $0 [OPTIONS] [BACKUP_FILE]"
      echo ""
      echo "Options:"
      echo "  --dry-run    Show what would be restored without doing it"
      echo "  --force      Overwrite existing files without asking"
      echo "  --help       Show this help"
      echo ""
      echo "If no BACKUP_FILE given, lists available backups."
      exit 0
      ;;
    *) BACKUP_FILE="$1"; shift ;;
  esac
done

# --- List available backups ---
if [ -z "$BACKUP_FILE" ]; then
  echo ""
  echo "== Available Backups =="
  echo ""
  
  if [ ! -d "$BACKUP_BASE" ]; then
    fail "No backups directory found: $BACKUP_BASE"
  fi
  
  BACKUPS=($(ls -1t "$BACKUP_BASE"/*.tar.gz 2>/dev/null))
  
  if [ ${#BACKUPS[@]} -eq 0 ]; then
    fail "No backups found in $BACKUP_BASE"
  fi
  
  echo "  # | Date                  | Size"
  echo "  ---+----------------------+-------"
  
  for i in "${!BACKUPS[@]}"; do
    local_file="${BACKUPS[$i]}"
    local_name=$(basename "$local_file")
    local_date=$(echo "$local_name" | sed 's/\.tar\.gz//' | sed 's/-/ /')
    local_size=$(du -h "$local_file" | cut -f1)
    echo "  $((i+1)) | $local_date | $local_size"
  done
  
  echo ""
  echo "Usage: $0 <backup_number_or_file>"
  echo "  Example: $0 1"
  echo "  Example: $0 $BACKUP_BASE/20260703-120000.tar.gz"
  echo ""
  exit 0
fi

# --- Find backup file ---
if [[ "$BACKUP_FILE" =~ ^[0-9]+$ ]]; then
  # Numeric index
  BACKUPS=($(ls -1t "$BACKUP_BASE"/*.tar.gz 2>/dev/null))
  INDEX=$((BACKUP_FILE - 1))
  if [ "$INDEX" -lt 0 ] || [ "$INDEX" -ge "${#BACKUPS[@]}" ]; then
    fail "Invalid backup number: $BACKUP_FILE"
  fi
  BACKUP_FILE="${BACKUPS[$INDEX]}"
fi

if [ ! -f "$BACKUP_FILE" ]; then
  fail "Backup file not found: $BACKUP_FILE"
fi

echo ""
echo "== Restore from Backup =="
echo ""
info "Backup: $(basename "$BACKUP_FILE")"

# --- Extract to temp dir ---
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

info "Extracting..."
tar -xzf "$BACKUP_FILE" -C "$TEMP_DIR" 2>/dev/null

# Find extracted directory
EXTRACTED=$(ls -1d "$TEMP_DIR"/*/ 2>/dev/null | head -1)
if [ -z "$EXTRACTED" ]; then
  fail "Invalid backup archive"
fi

# Show manifest
if [ -f "$EXTRACTED/manifest.txt" ]; then
  section "Backup Info"
  cat "$EXTRACTED/manifest.txt" | sed 's/^/  /'
fi

# --- Confirm ---
section "Files to Restore"

RESTORE_COUNT=0
  if [ -d "$EXTRACTED/$dir" ]; then
    for file in "$EXTRACTED/$dir"/*; do
      if [ -f "$file" ]; then
        local_name=$(basename "$file")
        case "$dir" in
          labwc) local_dest="$CONFIG_DIR/$local_name" ;;
          scripts) local_dest="$SCRIPT_DIR/$local_name" ;;
          dotfiles) local_dest="$SCRIPT_DIR/../dotfiles/$local_name" ;;
        esac
        
        if [ -f "$local_dest" ]; then
          if $FORCE; then
            info "  OVERWRITE: $local_dest"
          else
            info "  EXISTS:    $local_dest"
          fi
        else
          info "  NEW:       $local_dest"
        fi
        ((RESTORE_COUNT++))
      fi
    done
  fi
done

echo ""
if [ "$RESTORE_COUNT" -eq 0 ]; then
  fail "No files to restore"
fi

if ! $FORCE && ! $DRY_RUN; then
  read -rp "Restore $RESTORE_COUNT file(s)? [y/N] " ans
  if [[ ! "$ans" =~ ^[Yy] ]]; then
    info "Restore cancelled"
    exit 0
  fi
fi

# --- Restore files ---
section "Restoring"

RESTORED=0
  if [ -d "$EXTRACTED/$dir" ]; then
    for file in "$EXTRACTED/$dir"/*; do
      if [ -f "$file" ]; then
        local_name=$(basename "$file")
        case "$dir" in
          labwc) local_dest="$CONFIG_DIR/$local_name" ;;
          scripts) local_dest="$SCRIPT_DIR/$local_name" ;;
          dotfiles) local_dest="$SCRIPT_DIR/../dotfiles/$local_name" ;;
        esac
        
        if $DRY_RUN; then
          info "  Would restore: $local_dest"
        else
          mkdir -p "$(dirname "$local_dest")"
          cp "$file" "$local_dest"
          pass "Restored: $local_dest"
        fi
        ((RESTORED++))
      fi
    done
  fi
done

# --- Post-restore ---
if ! $DRY_RUN; then
  section "Post-Restore"
  
  # Make autostart executable
  if [ -f "$CONFIG_DIR/autostart" ]; then
    chmod +x "$CONFIG_DIR/autostart"
    pass "Made autostart executable"
  fi
  
  # Reload labwc if running
  if pgrep -x labwc &>/dev/null; then
    info "Reloading labwc config..."
    labwc --reconfigure 2>/dev/null && pass "labwc reloaded" || warn "Could not reload labwc"
  fi
fi

# --- Summary ---
echo ""
if $DRY_RUN; then
  echo -e "${YELLOW}${BOLD}Dry run complete${NC} — no changes made"
else
  echo -e "${GREEN}${BOLD}Restore complete${NC}"
  echo "  Files restored: $RESTORED"
  echo ""
  echo "Run ./scripts/validate.sh to verify setup"
fi
echo ""
