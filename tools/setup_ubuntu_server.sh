#!/bin/bash
# Setup NinaCat Checkpoint Generator on Ubuntu Server
# Run with: sudo bash setup_ubuntu_server.sh

set -e

echo "=========================================="
echo "NinaCat Checkpoint Generator - Ubuntu Setup"
echo "=========================================="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then
   echo "❌ Este script debe ejecutarse como root"
   echo "   sudo bash setup_ubuntu_server.sh"
   exit 1
fi

# Variables
INSTALL_DIR="/opt/ninacatcoin"
SERVICE_NAME="ninacatcoin-checkpoint-generator"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
VENV_DIR="${INSTALL_DIR}/.venv"

echo "📋 Configuración:"
echo "  Install dir: $INSTALL_DIR"
echo "  Service name: $SERVICE_NAME"
echo

# 1. Create ninacatcoin user
echo "1️⃣  Creando usuario 'ninacatcoin'..."
if ! id "ninacatcoin" &>/dev/null; then
    useradd -r -s /bin/bash -d "$INSTALL_DIR" ninacatcoin
    echo "   ✅ Usuario creado"
else
    echo "   ✅ Usuario ya existe"
fi

# 2. Create installation directory
echo "2️⃣  Creando directorio de instalación..."
if [ ! -d "$INSTALL_DIR" ]; then
    mkdir -p "$INSTALL_DIR"
    chown ninacatcoin:ninacatcoin "$INSTALL_DIR"
    chmod 750 "$INSTALL_DIR"
    echo "   ✅ Directorio creado"
else
    echo "   ✅ Directorio ya existe"
fi

# 3. Check Python3
echo "3️⃣  Verificando Python 3..."
if ! command -v python3 &> /dev/null; then
    echo "   ❌ Python 3 no instalado"
    echo "   Instalando: apt-get install -y python3 python3-pip python3-venv"
    apt-get update
    apt-get install -y python3 python3-pip python3-venv
fi
echo "   ✅ Python 3: $(python3 --version)"

# 4. Create virtual environment
echo "4️⃣  Creando virtual environment..."
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
    chown -R ninacatcoin:ninacatcoin "$VENV_DIR"
    echo "   ✅ Virtual environment creado"
else
    echo "   ✅ Virtual environment ya existe"
fi

# 5. Install Python dependencies
echo "5️⃣  Instalando dependencias Python..."
su - ninacatcoin -c "$VENV_DIR/bin/pip install --upgrade pip paramiko requests > /dev/null 2>&1"
echo "   ✅ Dependencias instaladas"

# 6. Setup systemd service
echo "6️⃣  Configurando systemd service..."
cat > "$SERVICE_FILE" << 'EOF'
[Unit]
Description=NinaCat Checkpoint Generator Daemon
Documentation=file:///opt/ninacatcoin/CHECKPOINT_SETUP.txt
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=ninacatcoin
WorkingDirectory=/opt/ninacatcoin
ExecStart=/opt/ninacatcoin/.venv/bin/python3 /opt/ninacatcoin/contrib/checkpoint_generator.py \
    --daemon-mode \
    --network TESTNET \
    --daemon-rpc http://localhost:29081 \
    --interval 1000 \
    --daemon-interval 7200 \
    --confirmations 50

Restart=always
RestartSec=60

StandardOutput=journal
StandardError=journal
SyslogIdentifier=checkpoint-gen

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/opt/ninacatcoin

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
echo "   ✅ Service configurado"

# 7. Copy scripts (if in current directory)
echo "7️⃣  Copiando scripts..."
if [ -f "contrib/checkpoint_generator.py" ]; then
    cp -r contrib "$INSTALL_DIR/"
    chown -R ninacatcoin:ninacatcoin "$INSTALL_DIR/contrib"
    echo "   ✅ Scripts copiados"
else
    echo "   ⚠️  contrib/checkpoint_generator.py no encontrado"
    echo "   Asegúrate de estar en el directorio ninacatcoin"
fi

echo
echo "=========================================="
echo "✅ SETUP COMPLETADO"
echo "=========================================="
echo
echo "🚀 Para iniciar el servicio:"
echo
echo "   sudo systemctl start $SERVICE_NAME"
echo
echo "📊 Para ver logs en tiempo real:"
echo
echo "   sudo journalctl -u $SERVICE_NAME -f"
echo
echo "⚙️  Para habilitar auto-start en reboot:"
echo
echo "   sudo systemctl enable $SERVICE_NAME"
echo
echo "📋 Para ver estado del servicio:"
echo
echo "   sudo systemctl status $SERVICE_NAME"
echo
echo "🛑 Para detener:"
echo
echo "   sudo systemctl stop $SERVICE_NAME"
echo
echo "=========================================="
