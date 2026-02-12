#!/bin/bash
# Script de prueba rápida del generador de checkpoints
# Uso: bash contrib/test_checkpoint_generator.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/checkpoint_generator.py"

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  NinaCat Checkpoint Generator - Test Suite                ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Test 1: Verificar Python
echo "[TEST 1] Verificando Python 3..."
if ! python3 --version > /dev/null 2>&1; then
    echo "✗ FALLO: Python 3 no encontrado"
    exit 1
fi
echo "✓ PASÓ: Python encontrado - $(python3 --version)"

# Test 2: Verificar script existe
echo ""
echo "[TEST 2] Verificando script..."
if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo "✗ FALLO: $PYTHON_SCRIPT no encontrado"
    exit 1
fi
echo "✓ PASÓ: Script encontrado"

# Test 3: Verificar sintaxis Python
echo ""
echo "[TEST 3] Verificando sintaxis Python..."
if ! python3 -m py_compile "$PYTHON_SCRIPT" 2>&1; then
    echo "✗ FALLO: Error de sintaxis en script"
    exit 1
fi
echo "✓ PASÓ: Sintaxis correcta"

# Test 4: Verificar dependencias
echo ""
echo "[TEST 4] Verificando dependencias..."
if ! python3 -c "import requests" 2>/dev/null; then
    echo "⚠ AVISO: 'requests' no instalado"
    echo "  Instalando: pip install requests"
    python3 -m pip install -q requests 2>/dev/null || python3 -m pip install requests
fi
echo "✓ PASÓ: Dependencias disponibles"

# Test 5: Verificar help funciona
echo ""
echo "[TEST 5] Verificando ayuda..."
if ! python3 "$PYTHON_SCRIPT" --help > /dev/null 2>&1; then
    echo "✗ FALLO: Script --help falló"
    exit 1
fi
echo "✓ PASÓ: Help funciona"

# Test 6: Verificar conexión a daemon (opcional)
echo ""
echo "[TEST 6] Verificando conexión a daemon (opcional)..."
if python3 -c "import requests; requests.post('http://localhost:19081/json_rpc', json={'jsonrpc':'2.0','method':'get_info','id':0}, timeout=2)" 2>/dev/null; then
    echo "✓ PASÓ: Daemon accesible en :19081"
else
    echo "⚠ AVISO: Daemon no accesible en :19081"
    echo "  (Esto es normal si ninacatcoind no está corriendo)"
fi

# Test 7: Ver versión script
echo ""
echo "[TEST 7] Verificando metadata..."
if grep -q "NinaCat Automatic Checkpoint Generator" "$PYTHON_SCRIPT"; then
    echo "✓ PASÓ: Script correctamente identificado"
else
    echo "✗ FALLO: Script metadata inválido"
    exit 1
fi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Todos los tests pasaron ✓                                ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Próximos pasos:"
echo "  1. Iniciar ninacatcoind:"
echo "     ./ninacatcoind --rpc-bind-ip 127.0.0.1 --rpc-bind-port 19081"
echo ""
echo "  2. Ejecutar generador una sola vez:"
echo "     python3 contrib/checkpoint_generator.py"
echo ""
echo "  3. O ejecutar como daemon:"
echo "     python3 contrib/checkpoint_generator.py --daemon-mode"
echo ""
echo "  4. Ver documentación:"
echo "     cat docs/AUTOMATIC_CHECKPOINTS.md"
echo ""
