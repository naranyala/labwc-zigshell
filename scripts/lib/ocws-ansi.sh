#!/bin/bash
# ocws-ansi.sh — Shared ANSI color definitions and output helpers
# Source this file: source "$(dirname "$0")/../lib/ocws-ansi.sh"

OCWS_RED='\033[0;31m'
OCWS_GREEN='\033[0;32m'
OCWS_YELLOW='\033[1;33m'
OCWS_CYAN='\033[0;36m'
OCWS_BOLD='\033[1m'
OCWS_DIM='\033[2m'
OCWS_NC='\033[0m'

ocws_pass() { echo -e "  ${OCWS_GREEN}✓${OCWS_NC} $*"; }
ocws_fail() { echo -e "  ${OCWS_RED}✗${OCWS_NC} $*"; exit 1; }
ocws_warn() { echo -e "  ${OCWS_YELLOW}⚠${OCWS_NC} $*"; }
ocws_info() { echo -e "\n${OCWS_CYAN}==>${OCWS_NC} $*"; }
ocws_header() { echo -e "\n${OCWS_BOLD}$*${OCWS_NC}"; }
