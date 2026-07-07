#!/bin/bash
OCWS_CFG="$HOME/.config/ocws/ocws.config"
LOCAL_CFG="dotfiles/ocws/ocws.config"

# Toggle locally
if grep -q "modes/topbar.config" "$LOCAL_CFG"; then
    sed -i 's|modes/topbar.config|modes/noctalia.config|g' "$LOCAL_CFG"
    echo "Switched to Noctalia/DankMaterialShell layout!"
else
    sed -i 's|modes/noctalia.config|modes/topbar.config|g' "$LOCAL_CFG"
    echo "Switched to standard Topbar layout!"
fi

# Apply to ~/.config if it exists
if [ -f "$OCWS_CFG" ]; then
    cp "$LOCAL_CFG" "$OCWS_CFG"
    killall -USR1 sfwbar 2>/dev/null || true
    echo "Live config reloaded."
fi
