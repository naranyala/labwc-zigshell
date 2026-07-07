#!/bin/bash
set -euo pipefail

# Crypto Ticker Prototype
# Fetches crypto prices and emits them to the OCWS Event Bus
# C-rewrite goal: A unified HTTP fetcher (libcurl) module in ocws-brokerd

# Configuration
COINS="bitcoin,ethereum,solana"
CURRENCY="usd"
INTERVAL=300 # 5 minutes

EMIT_CMD="ocws-emit"
if ! command -v $EMIT_CMD &> /dev/null; then
    EMIT_CMD="sh ../ocws-emit.sh" # fallback
fi

log() { echo "[$(date '+%H:%M:%S')] [Crypto] $*"; }

fetch_prices() {
    log "Fetching prices..."
    local url="https://api.coingecko.com/api/v3/simple/price?ids=${COINS}&vs_currencies=${CURRENCY}"
    
    # Use curl and jq to parse the json response
    local response
    if ! response=$(curl -s --max-time 10 "$url"); then
        log "Error: Failed to fetch API"
        return 1
    fi
    
    if [[ -z "$response" ]] || [[ "$response" == "{}" ]]; then
        log "Error: Empty response"
        return 1
    fi

    local btc_price=$(echo "$response" | jq -r '.bitcoin.usd // 0')
    local eth_price=$(echo "$response" | jq -r '.ethereum.usd // 0')
    
    # Emit to Event Bus (e.g. Applet.CryptoBTC)
    if [[ "$btc_price" != "0" ]]; then
        $EMIT_CMD Applet.CryptoBTC "$btc_price"
        log "BTC: $btc_price"
    fi
    if [[ "$eth_price" != "0" ]]; then
        $EMIT_CMD Applet.CryptoETH "$eth_price"
        log "ETH: $eth_price"
    fi
}

while true; do
    fetch_prices || true
    sleep $INTERVAL
done
