#!/bin/bash
# Setup script para sistema de checkpoints automáticos de NinaCat
# Ejecutar: bash setup_checkpoints.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR" && git rev-parse --show-toplevel 2>/dev/null || echo "$SCRIPT_DIR")"

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  NinaCat Automatic Checkpoint Generator - Setup           ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check Python
echo "[1/5] Verificando Python 3..."
if ! command -v python3 &> /dev/null; then
    echo "✗ Python 3 no encontrado. Instalar: apt install python3"
    exit 1
fi
echo "✓ Python 3 encontrado: $(python3 --version)"

# Install dependencies
echo ""
echo "[2/5] Instalando dependencias..."
python3 -m pip install -q requests 2>/dev/null || python3 -m pip install requests
echo "✓ Dependencias instaladas"

# Create directories
echo ""
echo "[3/5] Creando directorios..."
mkdir -p "$PROJECT_ROOT/contrib/checkpoints"
mkdir -p "$PROJECT_ROOT/build/checkpoints"
echo "✓ Directorios creados"

# Setup systemd service
echo ""
echo "[4/5] ¿Configurar como servicio systemd? (recomendado en Linux) [y/N]"
read -r -t 10 setup_systemd || setup_systemd="n"

if [[ "$setup_systemd" =~ ^[Yy]$ ]]; then
    echo "✓ Configurando servicio systemd..."
    
    SERVICE_FILE="/tmp/ninacatcoin-checkpoint-gen.service"
    cat > "$SERVICE_FILE" << EOF
[Unit]
Description=NinaCat Automatic Checkpoint Generator
After=network.target
Documentation=file://$PROJECT_ROOT/docs/AUTOMATIC_CHECKPOINTS.md

[Service]
Type=simple
ExecStart=$SCRIPT_DIR/run_checkpoint_generator.sh
Restart=always
RestartSec=30
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
    
    echo "Servicio guardado en: $SERVICE_FILE"
    echo "Para instalar (requiere sudo):"
    echo "  sudo cp $SERVICE_FILE /etc/systemd/system/"
    echo "  sudo systemctl daemon-reload"
    echo "  sudo systemctl enable ninacatcoin-checkpoint-gen"
    echo "  sudo systemctl start ninacatcoin-checkpoint-gen"
fi

# Create wrapper script
echo ""
echo "[5/5] Creando script wrapper..."
cat > "$PROJECT_ROOT/contrib/run_checkpoint_generator.sh" << 'EOF'
#!/bin/bash
# Wrapper para ejecución de checkpoint generator
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
exec python3 checkpoint_generator.py \
    --daemon-mode \
    --daemon-interval 3600 \
    --confirmations 100 \
    --daemon-rpc http://127.0.0.1:19081
EOF

chmod +x "$PROJECT_ROOT/contrib/run_checkpoint_generator.sh"
echo "✓ Script wrapper creado"

# Summary
echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Configuración completada                                 ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Uso:"
echo ""
echo "1. Generar checkpoints una vez:"
echo "   python3 contrib/checkpoint_generator.py"
echo ""
echo "2. Ejecutar como daemon (recomendado):"
echo "   python3 contrib/checkpoint_generator.py --daemon-mode"
echo ""
echo "3. Ver documentación completa:"
echo "   cat docs/AUTOMATIC_CHECKPOINTS.md"
echo ""
echo "Archivos generados:"
echo "  - contrib/checkpoint_generator.py (script principal)"
echo "  - contrib/run_checkpoint_generator.sh (wrapper)"
echo "  - docs/AUTOMATIC_CHECKPOINTS.md (documentación)"
echo ""
echo "✓ Setup completado"
