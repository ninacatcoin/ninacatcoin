#!/bin/bash
# Script para generar checkpoints automáticamente desde daemon RPC
# Usa contrib/update_checkpoints.py de ninacatcoin

RPC_URL="http://127.0.0.1:29081/json_rpc"
CHECKPOINTS_JSON="$HOME/.ninacatcoin/checkpoints.json"
LOG_FILE="/var/log/ninacatcoin-checkpoints-gen.log"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/tools/generate_checkpoints_from_rpc.py"

mkdir -p "$HOME/.ninacatcoin"
touch "$LOG_FILE"

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT GENERATION STARTED ==========" >> "$LOG_FILE"

# Verificar que el daemon está corriendo
if ! pgrep -x "ninacatcoind" > /dev/null; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR: ninacatcoind daemon is not running" >> "$LOG_FILE"
    exit 1
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Generating checkpoints from RPC: $RPC_URL" >> "$LOG_FILE"

# Generar checkpoints cada 10,000 bloques (ajusta las alturas según necesites)
python3 "$PYTHON_SCRIPT" \
    --rpc-url "$RPC_URL" \
    --heights "0,100,1000,10000" \
    --output-json "$CHECKPOINTS_JSON" \
    >> "$LOG_FILE" 2>&1

if [ $? -eq 0 ]; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Checkpoints generated successfully" >> "$LOG_FILE"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] File: $CHECKPOINTS_JSON" >> "$LOG_FILE"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Size: $(stat -c%s "$CHECKPOINTS_JSON" 2>/dev/null || stat -f%z "$CHECKPOINTS_JSON") bytes" >> "$LOG_FILE"
else
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR generating checkpoints" >> "$LOG_FILE"
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT GENERATION COMPLETED ==========" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
