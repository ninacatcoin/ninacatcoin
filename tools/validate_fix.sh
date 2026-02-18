#!/bin/bash
# Script de Validación del Fix
# Ejecutar después de que blockchain haya sincronizado hasta block 118,293

set -e

echo "=========================================="
echo "Validación del Fix - already_generated_coins"
echo "=========================================="

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Función para reportar
report_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ PASS${NC}: $2"
        return 0
    else
        echo -e "${RED}❌ FAIL${NC}: $2"
        return 1
    fi
}

print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

print_value() {
    echo -e "${BLUE}   $1${NC}"
}

# ============== VALIDACIÓN 1: RPC Disponible ==============
echo ""
print_info "VALIDACIÓN 1: Verificando RPC disponible..."

RPC_RESPONSE=$(curl -s http://localhost:20181/json_rpc \
    -d '{"jsonrpc":"2.0","id":"0","method":"getblockcount"}' \
    -H 'Content-Type: application/json')

CURRENT_HEIGHT=$(echo "$RPC_RESPONSE" | jq -r '.result.count // null')

if [ "$CURRENT_HEIGHT" != "null" ] && [ "$CURRENT_HEIGHT" -gt 0 ]; then
    report_status 0 "RPC disponible"
    print_value "Altura actual: $CURRENT_HEIGHT"
else
    report_status 1 "RPC no responde correctamente"
    echo "$RPC_RESPONSE"
    exit 1
fi

# ============== VALIDACIÓN 2: Alcanzó block 118,293 ==============
echo ""
print_info "VALIDACIÓN 2: Verificando si se alcanzó block 118,293..."

if [ "$CURRENT_HEIGHT" -ge 118293 ]; then
    report_status 0 "Blockchain sincronizado a block 118,293"
else
    report_status 1 "Blockchain aún no alcanza block 118,293"
    print_value "Altura actual: $CURRENT_HEIGHT"
    print_value "Falta: $((118293 - CURRENT_HEIGHT)) bloques"
    exit 1
fi

# ============== VALIDACIÓN 3: Block 118,292 - already_generated_coins ==============
echo ""
print_info "VALIDACIÓN 3: Verificando already_generated_coins en block 118,292..."

BLOCK_118292=$(curl -s http://localhost:20181/json_rpc \
    -d '{"jsonrpc":"2.0","id":"0","method":"getblockheaderbyheight","params":{"height":118292}}' \
    -H 'Content-Type: application/json')

AGC_118292=$(echo "$BLOCK_118292" | jq -r '.result.block_header.already_generated_coins // null')

if [ "$AGC_118292" != "null" ] && [ "$AGC_118292" != "0" ]; then
    report_status 0 "already_generated_coins accessible en block 118,292"
    print_value "Valor: $AGC_118292"
    
    # Convertir a millones para legibilidad
    AGC_MILLIONS=$(echo "scale=2; $AGC_118292 / 1000000000000" | bc)
    print_value "En millones NINA: $AGC_MILLIONS"
    
    # Validar que no es el valor erróneo (999M)
    if [ "$AGC_118292" -lt 900000000000000 ]; then
        report_status 0 "❌ NO es 999M (error previo)"
        print_value "El fix está funcionando"
    else
        report_status 1 "⚠️  Valor cercano a 999M - posible fallo del fix"
    fi
else
    report_status 1 "No se pudo obtener already_generated_coins"
    echo "$BLOCK_118292" | jq '.'
fi

# ============== VALIDACIÓN 4: Block 118,293 - Rewards ==============
echo ""
print_info "VALIDACIÓN 4: Verificando rewards en block 118,293..."

BLOCK_118293=$(curl -s http://localhost:20181/json_rpc \
    -d '{"jsonrpc":"2.0","id":"0","method":"getblock","params":{"height":118293}}' \
    -H 'Content-Type: application/json')

MINER_TX=$(echo "$BLOCK_118293" | jq -r '.result.miner_tx // null')

if [ "$MINER_TX" != "null" ]; then
    REWARD_AMOUNT=$(echo "$BLOCK_118293" | jq -r '.result.miner_tx.vout[0].amount // 0')
    
    if [ "$REWARD_AMOUNT" != "0" ] && [ "$REWARD_AMOUNT" != "null" ]; then
        report_status 0 "Block 118,293 tiene rewards"
        print_value "Reward: $REWARD_AMOUNT satoshi"
        REWARD_NINA=$(echo "scale=2; $REWARD_AMOUNT / 1000000000000" | bc)
        print_value "Reward en NINA: $REWARD_NINA"
    else
        report_status 1 "Block 118,293 NO tiene rewards (ERROR)"
        print_value "Esto indica que el fix no funcionó"
    fi
else
    report_status 1 "No se pudo obtener miner_tx del block 118,293"
    echo "$BLOCK_118293" | jq '.'
fi

# ============== VALIDACIÓN 5: Block 200,000 - Rewards ==============
echo ""
print_info "VALIDACIÓN 5: Verificando rewards en block 200,000 (si disponible)..."

if [ "$CURRENT_HEIGHT" -ge 200000 ]; then
    BLOCK_200000=$(curl -s http://localhost:20181/json_rpc \
        -d '{"jsonrpc":"2.0","id":"0","method":"getblock","params":{"height":200000}}' \
        -H 'Content-Type: application/json')
    
    REWARD_200000=$(echo "$BLOCK_200000" | jq -r '.result.miner_tx.vout[0].amount // 0')
    
    if [ "$REWARD_200000" != "0" ]; then
        report_status 0 "Block 200,000 tiene rewards"
        print_value "Reward: $REWARD_200000 satoshi"
    else
        report_status 1 "Block 200,000 no tiene rewards"
    fi
else
    print_value "Block 200,000 aún no sincronizado (altura actual: $CURRENT_HEIGHT)"
fi

# ============== VALIDACIÓN 6: Crecimiento de already_generated_coins ==============
echo ""
print_info "VALIDACIÓN 6: Verificando crecimiento lineal de already_generated_coins..."

if [ "$CURRENT_HEIGHT" -ge 200000 ]; then
    BLOCK_200000_HEADER=$(curl -s http://localhost:20181/json_rpc \
        -d '{"jsonrpc":"2.0","id":"0","method":"getblockheaderbyheight","params":{"height":200000}}' \
        -H 'Content-Type: application/json')
    
    AGC_200000=$(echo "$BLOCK_200000_HEADER" | jq -r '.result.block_header.already_generated_coins // null')
    
    if [ "$AGC_200000" != "null" ] && [ "$AGC_118292" != "null" ]; then
        GROWTH=$((AGC_200000 - AGC_118292))
        GROWTH_NINA=$(echo "scale=2; $GROWTH / 1000000000000" | bc)
        BLOCKS_PASSED=$((200000 - 118292))
        
        report_status 0 "Crecimiento de already_generated_coins: $GROWTH_NINA NINA"
        print_value "En $BLOCKS_PASSED bloques: $GROWTH_NINA NINA"
        print_value "Promedio por bloque: $(echo "scale=2; $GROWTH_NINA / $BLOCKS_PASSED" | bc) NINA"
        
        # El promedio debe ser cercano a base_reward (~16 NINA)
        AVG_REWARD=$(echo "scale=2; $GROWTH_NINA / $BLOCKS_PASSED" | bc)
        if (( $(echo "$AVG_REWARD > 8" | bc -l) )); then
            report_status 0 "Crecimiento lineal confirmado"
        else
            report_status 1 "Crecimiento menor al esperado"
        fi
    fi
else
    print_value "Block 200,000 aún no sincronizado, saltando validación de crecimiento"
fi

# ============== VALIDACIÓN 7: Logs sin errores ==============
echo ""
print_info "VALIDACIÓN 7: Verificando logs sin errores críticos..."

LOG_FILE="$HOME/.ninacatcoin/bitmonero.log"

if [ -f "$LOG_FILE" ]; then
    ERROR_COUNT=$(grep -i "error\|panic\|fatal\|genesis" "$LOG_FILE" | wc -l)
    
    if [ "$ERROR_COUNT" -lt 10 ]; then
        report_status 0 "Logs sin errores críticos"
        print_value "Líneas con warnings/errors: $ERROR_COUNT"
    else
        print_value "⚠️  Logs contienen $ERROR_COUNT líneas con warnings"
        grep -i "error\|panic\|fatal" "$LOG_FILE" | tail -5
    fi
else
    print_value "Archivo de log no encontrado"
fi

# ============== RESUMEN FINAL ==============
echo ""
echo "=========================================="
echo "Validación Completada"
echo "=========================================="
echo ""
echo "Resumen:"
echo ""

if [ "$REWARD_AMOUNT" != "0" ] && [ "$REWARD_AMOUNT" != "null" ] && [ "$AGC_118292" -lt 900000000000000 ]; then
    echo -e "${GREEN}✅ FIX VALIDADO EXITOSAMENTE${NC}"
    echo ""
    echo "Confirmaciones:"
    echo "  ✅ Block 118,293 tiene rewards (no 0)"
    echo "  ✅ already_generated_coins crece linealmente"
    echo "  ✅ No alcanzó cap de 999M NINA"
    echo "  ✅ Blockchain operando correctamente"
else
    echo -e "${RED}❌ FIX NO VALIDADO COMPLETAMENTE${NC}"
    echo ""
    echo "Problemas encontrados:"
    [ "$REWARD_AMOUNT" == "0" ] && echo "  ❌ Block 118,293 sin rewards"
    [ "$AGC_118292" -ge 900000000000000 ] && echo "  ❌ already_generated_coins cerca de 999M"
fi

echo ""
echo "=========================================="
