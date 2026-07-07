#!/bin/bash
set -euo pipefail
# Fetch and format labwc keybinds for sfwbar popup

CONFIG_FILE="$HOME/.config/labwc/rc.xml"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "No config found@"
    exit 0
fi

awk -F'[<>]' '
  /<keybind/ {
    key = "";
    match($0, /key="[^"]+"/);
    if(RSTART) {
      key = substr($0, RSTART+5, RLENGTH-6);
    }
    
    act = "";
    match($0, /name="[^"]+"/);
    if(RSTART) {
      act = substr($0, RSTART+6, RLENGTH-7);
    }
    
    cmd = "";
    if (act == "Execute") {
       match($0, /<command>[^<]+/);
       if (RSTART) {
         cmd = substr($0, RSTART+9, RLENGTH-9);
       }
    }
    
    if (key != "") {
      if (cmd != "") {
        printf "%-20s %s@", key, cmd;
      } else {
        printf "%-20s %s@", key, act;
      }
    }
  }
' "$CONFIG_FILE"
