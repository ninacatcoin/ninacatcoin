#!/usr/bin/env python3
"""
Script de prueba para Discord Webhook
Envía un mensaje de prueba al canal de Discord
"""

import json
import os
import urllib.request
import urllib.error
import sys

# Usar webhook del archivo de configuración o variable de entorno
webhook_url = os.getenv("NINACATCOIN_DISCORD_WEBHOOK", "").strip()

if not webhook_url:
    # Intentar leer del archivo de configuración
    try:
        with open("/etc/ninacatcoin/discord-bot.env", "r") as f:
            for line in f:
                if line.startswith("NINACATCOIN_DISCORD_WEBHOOK="):
                    webhook_url = line.split("=", 1)[1].strip()
                    break
    except:
        pass

if not webhook_url:
    print("❌ Error: No se encontró NINACATCOIN_DISCORD_WEBHOOK")
    print("Usa: export NINACATCOIN_DISCORD_WEBHOOK='tu_webhook_url'")
    sys.exit(1)

# Mensaje de prueba
test_message = """
🤖 **MENSAJE DE PRUEBA - NINACATCOIN BOT**

✅ Bot de Discord conectado correctamente
✅ Webhook funcionando
✅ Servidor: Ninacatcoin Testnet
✅ Timestamp: """+ str(os.popen("date").read().strip()) +"""

**Estado del Sistema:**
- Daemon: ✅ Corriendo
- Explorer: ✅ Corriendo
- Discord Bot: ✅ Activo
- Minería: ⏳ Próxima a iniciar

Este es un mensaje de prueba. El bot está listo para monitorear minería.
"""

# Enviar webhook
try:
    payload = json.dumps({"content": test_message}).encode("utf-8")
    req = urllib.request.Request(
        webhook_url,
        data=payload,
        headers={"Content-Type": "application/json", "User-Agent": "NinacatcoinBot/1.0"},
    )
    with urllib.request.urlopen(req, timeout=10) as response:
        status = response.status
        print(f"✅ Mensaje enviado correctamente (HTTP {status})")
        print(f"📍 Webhook URL: {webhook_url[:50]}...")
except urllib.error.URLError as e:
    print(f"❌ Error de conexión: {e}")
    sys.exit(1)
except Exception as e:
    print(f"❌ Error: {e}")
    sys.exit(1)
