#!/bin/bash
# Script para generar checkpoints y subirlos automáticamente al hosting
# Via SFTP a https://ninacatcoin.es/checkpoints/checkpoints.json

# Credenciales SFTP
SFTP_HOST="access-5019408946.webspace-host.com"
SFTP_PORT="22"
SFTP_USER="su318613"
SFTP_PASS="nina1986/66"  # La barra será escapada en la URL
SFTP_REMOTE_PATH="/checkpoints"

# Escapar caracteres especiales en la contraseña para URL
SFTP_PASS_ESCAPED=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$SFTP_PASS', safe=''))")

# Archivos locales
LOCAL_CHECKPOINTS="$HOME/.ninacatcoin/checkpoints.json"
LOG_FILE="/var/log/ninacatcoin-checkpoints-upload.log"

mkdir -p "$(dirname "$LOG_FILE")"
touch "$LOG_FILE"

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT UPLOAD STARTED ==========" >> "$LOG_FILE"

# Verificar que el archivo existe
if [ ! -f "$LOCAL_CHECKPOINTS" ]; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR: Checkpoints file not found at $LOCAL_CHECKPOINTS" >> "$LOG_FILE"
    exit 1
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Uploading checkpoints to SFTP..." >> "$LOG_FILE"
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Host: $SFTP_HOST:$SFTP_PORT" >> "$LOG_FILE"
echo "[$(date '+%Y-%m-%d %H:%M:%S')] User: $SFTP_USER" >> "$LOG_FILE"
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Remote path: $SFTP_REMOTE_PATH" >> "$LOG_FILE"

# Usar lftp para subir via SFTP (requiere lftp instalado)
if command -v lftp &> /dev/null; then
    lftp -e "set sftp:auto-confirm yes; set net:max-retries 2; set net:timeout 10; open sftp://$SFTP_USER:$SFTP_PASS_ESCAPED@$SFTP_HOST:$SFTP_PORT; cd $SFTP_REMOTE_PATH; put $LOCAL_CHECKPOINTS; bye" >> "$LOG_FILE" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Checkpoints uploaded successfully to $SFTP_HOST$SFTP_REMOTE_PATH" >> "$LOG_FILE"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] URL: https://ninacatcoin.es/checkpoints/checkpoints.json" >> "$LOG_FILE"
        FILE_SIZE=$(stat -c%s "$LOCAL_CHECKPOINTS" 2>/dev/null || stat -f%z "$LOCAL_CHECKPOINTS")
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] File size: $FILE_SIZE bytes" >> "$LOG_FILE"
    else
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR uploading checkpoints" >> "$LOG_FILE"
    fi
else
    # Fallback: usar sftp directamente
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] lftp not found, trying sftp..." >> "$LOG_FILE"
    
    (
        echo "cd $SFTP_REMOTE_PATH"
        echo "put $LOCAL_CHECKPOINTS"
        echo "bye"
    ) | sftp -o StrictHostKeyChecking=no -P $SFTP_PORT "$SFTP_USER@$SFTP_HOST" >> "$LOG_FILE" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Checkpoints uploaded successfully" >> "$LOG_FILE"
    else
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ ERROR uploading checkpoints" >> "$LOG_FILE"
    fi
fi

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ========== CHECKPOINT UPLOAD COMPLETED ==========" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"
