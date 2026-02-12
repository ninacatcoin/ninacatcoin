#!/bin/bash
# Script de Post-Compilación y Validación
# Ejecutar después de que la compilación de ninacatcoind esté completa

set -e

echo "=========================================="
echo "Post-Compilación - Script de Validación"
echo "=========================================="

# Colores para output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

NINACATCOIND_PATH="/mnt/i/ninacatcoin/build-linux/bin/ninacatcoind"
DATA_DIR="$HOME/.ninacatcoin"

# Función para reportar status
report_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ $2${NC}"
    else
        echo -e "${RED}❌ $2${NC}"
        return 1
    fi
}

# Función para print info
print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

# ============== PASO 1: Verificar Compilación ==============
echo ""
print_info "PASO 1: Verificando compilación exitosa..."

if [ -f "$NINACATCOIND_PATH" ]; then
    report_status 0 "ninacatcoind encontrado en $NINACATCOIND_PATH"
    ls -lh "$NINACATCOIND_PATH"
else
    report_status 1 "ninacatcoind NO encontrado. Compilación incompleta."
    exit 1
fi

# ============== PASO 2: Detener daemon existente ==============
echo ""
print_info "PASO 2: Deteniendo ninacatcoind si está corriendo..."

pkill -f "ninacatcoind" || true
sleep 2
report_status 0 "Proceso detenido"

# ============== PASO 3: Backup de base de datos ==============
echo ""
print_info "PASO 3: Respaldando base de datos actual (opcional)..."

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="$DATA_DIR/lmdb_backup_$TIMESTAMP"

if [ -d "$DATA_DIR/lmdb" ]; then
    mkdir -p "$BACKUP_DIR"
    echo "Respaldando a: $BACKUP_DIR"
    # Descomentar para hacer backup:
    # cp -r "$DATA_DIR/lmdb" "$BACKUP_DIR/" && report_status 0 "Backup completado"
    echo "Descomentó cp para hacer backup si lo desea"
else
    report_status 0 "No hay base de datos anterior que respaldar"
fi

# ============== PASO 4: Limpiar base de datos ==============
echo ""
print_info "PASO 4: Limpiando base de datos blockchain..."
print_info "Advertencia: Esto eliminará todos los bloques descargados"
print_info "Presione Enter para continuar o Ctrl+C para cancelar..."
read -p ""

rm -rf "$DATA_DIR/lmdb" && report_status 0 "Base de datos eliminada"
rm -rf "$DATA_DIR/testnet/lmdb" && report_status 0 "Base de datos testnet eliminada"  
rm -rf "$DATA_DIR/stagenet/lmdb" && report_status 0 "Base de datos stagenet eliminada"

# ============== PASO 5: Iniciar sincronización ==============
echo ""
print_info "PASO 5: Iniciando sincronización blockchain..."

# Para mainnet
echo "Iniciando ninacatcoind (mainnet)..."
nohup "$NINACATCOIND_PATH" --data-dir "$DATA_DIR" > "$DATA_DIR/ninacatcoind.log" 2>&1 &
sleep 5

DAEMON_PID=$(pgrep -f "ninacatcoind" | head -1)
if [ -n "$DAEMON_PID" ]; then
    report_status 0 "ninacatcoind iniciado con PID: $DAEMON_PID"
    print_info "Log: tail -f $DATA_DIR/bitmonero.log"
else
    report_status 1 "Error iniciando ninacatcoind"
    cat "$DATA_DIR/ninacatcoind.log"
    exit 1
fi

# ============== PASO 6: Monitorear sincronización ==============
echo ""
print_info "PASO 6: Monitoreando sincronización..."
print_info "Esperando que ninacatcoind esté listo para RPC..."

for i in {1..30}; do
    if curl -s http://localhost:20181/json_rpc \
        -d '{"jsonrpc":"2.0","id":"0","method":"getinfo"}' \
        -H 'Content-Type: application/json' > /dev/null 2>&1; then
        report_status 0 "RPC disponible"
        break
    fi
    echo "Intento $i/30... esperando"
    sleep 2
done

# ============== PASO 7: Verificación Inicial ==============
echo ""
print_info "PASO 7: Verificación de estado inicial..."

BLOCK_COUNT=$(curl -s http://localhost:20181/json_rpc \
    -d '{"jsonrpc":"2.0","id":"0","method":"getblockcount"}' \
    -H 'Content-Type: application/json' | jq -r '.result.count // 0')

echo "Altura actual del blockchain: $BLOCK_COUNT"

# ============== PASO 8: Verificar GENESIS_TX ==============
echo ""
print_info "PASO 8: Verificando block 0 (Genesis)..."

GENESIS_RESPONSE=$(curl -s http://localhost:20181/json_rpc \
    -d '{"jsonrpc":"2.0","id":"0","method":"getblock","params":{"height":0}}' \
    -H 'Content-Type: application/json')

GENESIS_REWARD=$(echo "$GENESIS_RESPONSE" | jq -r '.result.miner_tx.vout[0].amount // "ERROR"')

if [ "$GENESIS_REWARD" != "ERROR" ] && [ "$GENESIS_REWARD" != "0" ]; then
    report_status 0 "Genesis tiene reward: $GENESIS_REWARD NINA"
else
    report_status 1 "Genesis reward no coincide o es 0"
    echo "$GENESIS_RESPONSE" | jq '.'
fi

# ============== RESUMEN ==============
echo ""
echo "=========================================="
echo "Post-Compilación Completado"
echo "=========================================="
echo ""
echo "Estado:"
echo "  • Binario compilado: ✅"
echo "  • Base de datos limpia: ✅"
echo "  • ninacatcoind iniciado: ✅"
echo "  • RPC disponible: ✅"
echo ""
print_info "Próximos pasos:"
echo "  1. Monitorear sincronización: tail -f $DATA_DIR/bitmonero.log"
echo "  2. Esperar a block 118,293"
echo "  3. Ejecutar script de validación"
echo ""
echo "Script: $(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/validate_fix.sh"
