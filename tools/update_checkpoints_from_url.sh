#!/bin/bash
# Script para descargar checkpoints automáticamente desde URL
# Se ejecuta cada 6 horas via cron

CHECKPOINT_URL="https://ninacatcoin.es/checkpoints/checkpoints.json"
LOCAL_FILE="$HOME/.ninacatcoin/checkpoints.json"
DAEMON_DIR="$HOME/.ninacatcoin"
LOG_FILE="/var/log/ninacatcoin-checkpoints.log"

# Crear directorio si no existe
mkdir -p "$DAEMON_DIR"

# Crear log si no existe
touch "$LOG_FILE"

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT UPDATE STARTED ==========" >> "$LOG_FILE"

# Descargar JSON desde URL
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Downloading checkpoints from: $CHECKPOINT_URL" >> "$LOG_FILE"
curl -f -s -o "$LOCAL_FILE.tmp" "$CHECKPOINT_URL" 2>> "$LOG_FILE"

if [ $? -eq 0 ]; then
    # Validar que sea JSON válido
    if python3 -m json.tool "$LOCAL_FILE.tmp" > /dev/null 2>&1; then
        # Reemplazar archivo anterior
        mv "$LOCAL_FILE.tmp" "$LOCAL_FILE"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Checkpoints updated successfully" >> "$LOG_FILE"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] File size: $(stat -f%z "$LOCAL_FILE" 2>/dev/null || stat -c%s "$LOCAL_FILE") bytes" >> "$LOG_FILE"
        
        # Contar checkpoints
        CHECKPOINT_COUNT=$(python3 -c "import json; f=open('$LOCAL_FILE'); d=json.load(f); print(len(d.get('hashlines', [])))" 2>/dev/null)
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] Total checkpoints loaded: $CHECKPOINT_COUNT" >> "$LOG_FILE"
        
        # Reload ninacatcoind si está corriendo
        if pgrep -x "ninacatcoind" > /dev/null; then
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] Daemon is running, checkpoints will be loaded on next connection/sync" >> "$LOG_FILE"
        else
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] Daemon is not running" >> "$LOG_FILE"
        fi
    else
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR: Downloaded file is not valid JSON" >> "$LOG_FILE"
        rm -f "$LOCAL_FILE.tmp"
    fi
else
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR: Failed to download checkpoints from URL" >> "$LOG_FILE"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Using existing checkpoints if available" >> "$LOG_FILE"
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT UPDATE COMPLETED ==========" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
