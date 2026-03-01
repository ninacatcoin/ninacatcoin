#!/usr/bin/env python3
"""
NINA AI Discord Chat Bot — NinaCatCoin
=======================================
Connects Discord chat to LLaMA 3.2 3B running via llama-server.

Architecture:
  User writes in Discord → Bot receives → Sends to llama-server HTTP API
  → LLM generates response → Firewall checks output → Bot replies in Discord

Security:
  - Input firewall: blocks financial commands, jailbreaks, private key requests
  - Output firewall: blocks addresses, keys, dangerous instructions
  - Rate limiting: 1 message per 15 seconds per user
  - NINA system prompt: hardcoded, cannot be overridden
  - Model is frozen: no learning from conversations

Usage:
  1. Start llama-server: llama-server -m model.gguf --port 8080 -ngl 0
  2. Start bot: python nina_bot.py
  
Config via environment variables or .env file:
  NINA_DISCORD_TOKEN=your_bot_token
  NINA_LLM_URL=http://127.0.0.1:8080
  NINA_CHANNEL_ID=channel_id (optional, restricts to one channel)

Copyright (c) 2026, The NinaCatCoin Project
"""

import os
import sys
import time
import re
import json
import asyncio
import logging
from typing import Optional

try:
    import discord
    from discord.ext import commands
except ImportError:
    print("ERROR: discord.py no instalado. Ejecuta:")
    print("  pip install discord.py")
    sys.exit(1)

try:
    import aiohttp
except ImportError:
    print("ERROR: aiohttp no instalado. Ejecuta:")
    print("  pip install aiohttp")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════

# Try loading from .env file
def load_env():
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if os.path.exists(env_path):
        with open(env_path) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    key, val = line.split("=", 1)
                    os.environ.setdefault(key.strip(), val.strip())

load_env()

DISCORD_TOKEN = os.environ.get("NINA_DISCORD_TOKEN", "")
LLM_SERVER_URL = os.environ.get("NINA_LLM_URL", "http://127.0.0.1:8080")
DAEMON_RPC_URL = os.environ.get("NINA_DAEMON_RPC", "http://127.0.0.1:19081")
ALLOWED_CHANNEL_ID = os.environ.get("NINA_CHANNEL_ID", "")  # Empty = all channels
MAX_RESPONSE_TOKENS = 80
RATE_LIMIT_SECONDS = 15  # Min seconds between messages per user
MAX_INPUT_LENGTH = 500    # Max chars per user message

# ═══════════════════════════════════════════════════════════════════════
# NINA SYSTEM PROMPT (HARDCODED — CANNOT BE OVERRIDDEN)
# ═══════════════════════════════════════════════════════════════════════

NINA_SYSTEM_PROMPT_EN = """You are NINA, NinaCatCoin AI. Reply in 1-2 short sentences only.
FACTS: NinaCatCoin is based on Monero/CryptoNote. Mining: RandomX (CPU only). Block time: ~2 min. Privacy: ring signatures + stealth addresses + RingCT. Difficulty: LWMA algorithm. NINA AI monitors network health.
If you don't know something, say "I don't have that information." No financial advice. No keys/addresses."""

NINA_SYSTEM_PROMPT_ES = """Eres NINA, IA de NinaCatCoin. Responde en 1-2 frases cortas.
DATOS: NinaCatCoin está basado en Monero/CryptoNote. Minería: RandomX (solo CPU). Tiempo de bloque: ~2 min. Privacidad: firmas de anillo + direcciones stealth + RingCT. Dificultad: algoritmo LWMA. NINA AI monitorea la salud de la red.
Si no sabes algo, di "No tengo esa información." Sin consejos financieros. Sin claves/direcciones."""


def detect_language(text: str) -> str:
    """Simple language detection: Spanish vs English."""
    es_words = ['hola', 'cómo', 'como', 'qué', 'que', 'está', 'esta', 'cuál', 'cual',
                'puedes', 'dime', 'eres', 'soy', 'tienes', 'puede', 'para', 'funciona',
                'minería', 'seguridad', 'privacidad', 'algoritmo', 'explica', 'gracias',
                'buenos', 'buenas', 'sabes', 'hablar', 'minar', 'cuánto', 'cuanto']
    text_lower = text.lower()
    es_count = sum(1 for w in es_words if w in text_lower)
    return 'es' if es_count >= 1 else 'en'


def get_system_prompt(lang: str) -> str:
    """Return system prompt in user's detected language."""
    return NINA_SYSTEM_PROMPT_ES if lang == 'es' else NINA_SYSTEM_PROMPT_EN

# ═══════════════════════════════════════════════════════════════════════
# INPUT FIREWALL
# ═══════════════════════════════════════════════════════════════════════

INPUT_BLOCKED_PATTERNS = [
    # Jailbreak attempts
    r"ignore.*(?:previous|above|system|all).*(?:instruct|prompt|rules)",
    r"you\s*are\s*now\s*(?:a|an)\s*(?:different|new|unrestricted)",
    r"pretend\s*(?:you|to\s*be)",
    r"DAN\s*mode",
    r"developer\s*mode",
    r"bypass.*(?:filter|safety|restrict)",
    r"act\s*as\s*(?:if|though)\s*you\s*(?:have|had)\s*no",
    r"forget.*(?:everything|rules|instructions)",
    
    # Financial exploitation
    r"send\s*(?:me|to|coins|tokens|nina|crypto)",
    r"transfer\s*(?:funds|coins|tokens)",
    r"(?:my|a|the)\s*private\s*key",
    r"(?:my|a|the)\s*seed\s*phrase",
    r"(?:my|a|the)\s*mnemonic",
    r"generate.*(?:address|wallet|key)",
    r"sign\s*(?:a\s*)?transaction",
    r"create.*(?:wallet|account)",
    
    # Attack assistance
    r"(?:how\s*to|help\s*me)\s*(?:hack|exploit|attack|ddos|crash)",
    r"(?:51|fifty.?one)\s*(?:percent|%)\s*attack",
    r"double\s*spend",
    r"selfish\s*mining",
]

INPUT_BLOCKED_RE = [re.compile(p, re.IGNORECASE) for p in INPUT_BLOCKED_PATTERNS]


def check_input_firewall(text: str) -> Optional[str]:
    """Returns rejection reason if blocked, None if OK."""
    for i, pattern in enumerate(INPUT_BLOCKED_RE):
        if pattern.search(text):
            return f"blocked_input_pattern_{i}"
    
    # Check for potential private keys (64 hex chars)
    if re.search(r'[0-9a-fA-F]{64}', text):
        return "contains_potential_private_key"
    
    # Check for potential crypto addresses
    if re.search(r'[45][1-9A-HJ-NP-Za-km-z]{94}', text):
        return "contains_crypto_address"
    
    return None


# ═══════════════════════════════════════════════════════════════════════
# OUTPUT FIREWALL
# ═══════════════════════════════════════════════════════════════════════

OUTPUT_BLOCKED_PATTERNS = [
    r"[45][1-9A-HJ-NP-Za-km-z]{94}",      # CryptoNote addresses
    r"[0-9a-fA-F]{64}",                     # Private keys / tx hashes
    r"(?:send|transfer|sign)\s*(?:coins|transaction|tx)",
    r"(?:private|secret)\s*key\s*(?:is|:)",
    r"seed\s*phrase\s*(?:is|:)",
    r"sudo\s+",
    r"rm\s+-rf",
    r"exec\s*\(",
    r"system\s*\(",
]

OUTPUT_BLOCKED_RE = [re.compile(p, re.IGNORECASE) for p in OUTPUT_BLOCKED_PATTERNS]

SAFE_FALLBACK = "🐱 I can't provide that information for security reasons. Ask me about NinaCatCoin's network, mining, or security instead!"


def sanitize_output(text: str) -> str:
    """Check LLM output and sanitize if dangerous."""
    for pattern in OUTPUT_BLOCKED_RE:
        if pattern.search(text):
            return SAFE_FALLBACK
    return text


# ═══════════════════════════════════════════════════════════════════════
# CANNED RESPONSES & FAQ (avoid LLM hallucinations with real facts)
# ═══════════════════════════════════════════════════════════════════════

import random

GREETINGS_EN = [
    "🐱 Hey there! I'm NINA, the AI guardian of NinaCatCoin. Ask me about mining, privacy, or network security!",
    "🐱 Hello! I'm NINA, watching over the NinaCatCoin network. What would you like to know?",
    "🐱 Hi! I'm NINA, NinaCatCoin's AI. I can help with questions about RandomX mining, CryptoNote privacy, and network health!",
]

GREETINGS_ES = [
    "🐱 ¡Hola! Soy NINA, la IA guardiana de NinaCatCoin. ¡Pregúntame sobre minería, privacidad o seguridad de red!",
    "🐱 ¡Hola! Soy NINA, vigilando la red NinaCatCoin. ¿Qué te gustaría saber?",
    "🐱 ¡Hey! Soy NINA, la IA de NinaCatCoin. Puedo ayudarte con minería RandomX, privacidad CryptoNote y salud de red.",
]

# ─── FAQ: Correct facts about NinaCatCoin ───
FAQ = [
    {
        "patterns": [r"(?:qu[eé]\s*es|what\s*is|what'?s)\s*(?:nina\s*cat\s*coin|ninacatcoin|nina\s*coin|ncoin|esta\s*crypto|this\s*crypto)"],
        "en": "🐱 NinaCatCoin is a privacy-focused cryptocurrency based on Monero/CryptoNote technology. It uses RandomX for CPU mining, has ~2 minute block times, and features NINA AI for intelligent network monitoring and security.",
        "es": "🐱 NinaCatCoin es una criptomoneda enfocada en privacidad basada en tecnología Monero/CryptoNote. Usa RandomX para minería CPU, tiene bloques cada ~2 minutos e incluye NINA AI para monitoreo inteligente y seguridad de red.",
    },
    {
        "patterns": [r"(?:qu[eé]|what)\s*(?:es|is)\s*(?:randomx|random\s*x)", r"(?:algoritmo|algorithm)\s*(?:de\s*)?(?:miner[ií]a|mining)"],
        "en": "🐱 RandomX is NinaCatCoin's mining algorithm. It's designed for CPU mining and resistant to ASICs and GPUs, making mining fairer and more decentralized.",
        "es": "🐱 RandomX es el algoritmo de minería de NinaCatCoin. Está diseñado para minería con CPU y es resistente a ASICs y GPUs, haciendo la minería más justa y descentralizada.",
    },
    {
        "patterns": [r"(?:c[oó]mo|how)\s*(?:minar|mine|mining|mino)", r"(?:puedo|can\s*i)\s*min(?:ar|e)", r"empezar.*min"],
        "en": "🐱 To mine NinaCatCoin you need: 1) Download the NinaCatCoin daemon, 2) Sync the blockchain, 3) Use XMRig or the built-in miner with RandomX algorithm. Any modern CPU works — no GPU needed!",
        "es": "🐱 Para minar NinaCatCoin necesitas: 1) Descargar el daemon NinaCatCoin, 2) Sincronizar la blockchain, 3) Usar XMRig o el minero integrado con algoritmo RandomX. ¡Cualquier CPU moderna sirve, no necesitas GPU!",
    },
    {
        "patterns": [r"privac(?:idad|y)", r"(?:an[oó]nim|anonym)", r"(?:ring\s*signature|firma\s*de\s*anillo)", r"stealth\s*address"],
        "en": "🐱 NinaCatCoin provides privacy through CryptoNote technology: ring signatures hide the sender, stealth addresses protect the receiver, and RingCT conceals transaction amounts. All transactions are private by default.",
        "es": "🐱 NinaCatCoin proporciona privacidad mediante tecnología CryptoNote: las firmas de anillo ocultan al emisor, las direcciones stealth protegen al receptor y RingCT oculta los montos. Todas las transacciones son privadas por defecto.",
    },
    {
        "patterns": [r"(?:qu[eé]|what)\s*(?:es|is)\s*(?:cryptonote|crypto\s*note)"],
        "en": "🐱 CryptoNote is the privacy protocol that NinaCatCoin is built on (same as Monero). It provides ring signatures, stealth addresses, and RingCT for fully private transactions.",
        "es": "🐱 CryptoNote es el protocolo de privacidad sobre el que está construido NinaCatCoin (igual que Monero). Proporciona firmas de anillo, direcciones stealth y RingCT para transacciones totalmente privadas.",
    },
    {
        "patterns": [r"(?:qu[eé]|what)\s*(?:es|is)\s*(?:nina\s*ai|la\s*ia|the\s*ai|tu\s*ia)", r"(?:c[oó]mo|how)\s*(?:funciona|works?)\s*(?:la\s*ia|nina\s*ai|the\s*ai|tu\s*ai)"],
        "en": "🐱 NINA AI is the built-in artificial intelligence of NinaCatCoin. I monitor network health, detect attacks (51%, selfish mining, spam), analyze the mempool, and help operators make informed decisions. I use embedded ML + LLaMA 3.2.",
        "es": "🐱 NINA AI es la inteligencia artificial integrada de NinaCatCoin. Monitoreo la salud de la red, detecto ataques (51%, minería egoísta, spam), analizo el mempool y ayudo a los operadores a tomar decisiones informadas. Uso ML embebido + LLaMA 3.2.",
    },
    {
        "patterns": [r"(?:tiempo|time)\s*(?:de\s*)?(?:bloque|block)", r"block\s*time"],
        "en": "🐱 NinaCatCoin has a target block time of approximately 2 minutes. Block difficulty is adjusted using the LWMA (Linear Weighted Moving Average) algorithm.",
        "es": "🐱 NinaCatCoin tiene un tiempo de bloque objetivo de aproximadamente 2 minutos. La dificultad se ajusta usando el algoritmo LWMA (Linear Weighted Moving Average).",
    },
    {
        "patterns": [r"(?:dificultad|difficulty)", r"lwma"],
        "en": "🐱 NinaCatCoin uses LWMA (Linear Weighted Moving Average) for difficulty adjustment, ensuring blocks stay near the 2-minute target even with hashrate fluctuations.",
        "es": "🐱 NinaCatCoin usa LWMA (Linear Weighted Moving Average) para ajustar la dificultad, asegurando que los bloques se mantengan cerca del objetivo de 2 minutos incluso con fluctuaciones de hashrate.",
    },
    {
        "patterns": [r"(?:segur(?:idad|ity)|security|safe)", r"(?:ataque|attack)", r"(?:proteg|protect)"],
        "en": "🐱 NinaCatCoin has multiple security layers: NINA AI monitoring, attack detection (51% attacks, selfish mining, DDoS), LLM firewall, constitutional rules, and CryptoNote privacy. The network is designed to be resilient and self-defending.",
        "es": "🐱 NinaCatCoin tiene múltiples capas de seguridad: monitoreo NINA AI, detección de ataques (51%, minería egoísta, DDoS), firewall LLM, reglas constitucionales y privacidad CryptoNote. La red está diseñada para ser resiliente y auto-defenderse.",
    },
    {
        "patterns": [r"monero", r"(?:diferencia|difference|vs|versus)"],
        "en": "🐱 NinaCatCoin is based on Monero's CryptoNote technology but adds NINA AI — an integrated artificial intelligence that monitors the network, detects threats, and helps operators. Same privacy, added intelligence!",
        "es": "🐱 NinaCatCoin está basado en la tecnología CryptoNote de Monero pero añade NINA AI — una inteligencia artificial integrada que monitorea la red, detecta amenazas y ayuda a los operadores. ¡Misma privacidad, inteligencia añadida!",
    },
    {
        "patterns": [r"(?:wallet|billetera|monedero)", r"(?:descargar|download)"],
        "en": "🐱 You can find NinaCatCoin wallet and daemon on the official GitHub repository: github.com/ninacatcoin/ninacatcoin. Build from source for maximum security!",
        "es": "🐱 Puedes encontrar la wallet y daemon de NinaCatCoin en el repositorio oficial de GitHub: github.com/ninacatcoin/ninacatcoin. ¡Compila desde el código fuente para máxima seguridad!",
    },
    {
        "patterns": [r"(?:mempool|mem\s*pool)"],
        "en": "🐱 The mempool is where pending transactions wait to be included in a block. NINA AI monitors the mempool health, detects spam attacks, and analyzes transaction patterns.",
        "es": "🐱 El mempool es donde las transacciones pendientes esperan a ser incluidas en un bloque. NINA AI monitorea la salud del mempool, detecta ataques de spam y analiza patrones de transacciones.",
    },
    {
        "patterns": [r"(?:gracias|thanks?|thank\s*you|thx)"],
        "en": "🐱 You're welcome! I'm always here to help with NinaCatCoin questions. Stay safe out there!",
        "es": "🐱 ¡De nada! Siempre estoy aquí para ayudar con preguntas sobre NinaCatCoin. ¡Mantente seguro!",
    },
    {
        "patterns": [r"(?:adi[oó]s|bye|see\s*you|hasta\s*luego|chao|nos\s*vemos)"],
        "en": "🐱 Goodbye! Remember: stay safe, mine responsibly, and keep your keys private! See you around!",
        "es": "🐱 ¡Adiós! Recuerda: mantente seguro, mina responsablemente y guarda tus claves en privado. ¡Nos vemos!",
    },
]

# Compile FAQ patterns
for faq in FAQ:
    faq["_compiled"] = [re.compile(p, re.IGNORECASE) for p in faq["patterns"]]

GREETING_PATTERNS = re.compile(
    r'^(?:h(?:i|ello|ey|ola)|buenos?\s*(?:d[ií]as|tardes|noches)|'
    r'(?:how|como|cómo)\s*(?:are|est[aá]s)|what\'?s\s*up|sup|'
    r'good\s*(?:morning|afternoon|evening)|'
    r'saludos|buenas|greetings|yo|hey\s*nina)[\s?!.,]*$',
    re.IGNORECASE
)

IDENTITY_PATTERNS = re.compile(
    r'(?:who|what)\s*(?:are|r)\s*(?:you|u)|'
    r'(?:qui[eé]n|qu[eé])\s*(?:eres|sos)|'
    r'(?:pres[eé]ntate|introduce\s*yourself)',
    re.IGNORECASE
)

IDENTITY_EN = "🐱 I'm NINA, the AI guardian of NinaCatCoin! I monitor network health, detect attacks, and help with questions about RandomX mining and CryptoNote privacy."
IDENTITY_ES = "🐱 ¡Soy NINA, la IA guardiana de NinaCatCoin! Monitoreo la salud de la red, detecto ataques y ayudo con preguntas sobre minería RandomX y privacidad CryptoNote."


def get_canned_response(text: str, lang: str) -> Optional[str]:
    """Return a canned/FAQ response, or None to use LLM for truly unknown questions."""
    # Greetings
    if GREETING_PATTERNS.match(text):
        return random.choice(GREETINGS_ES if lang == 'es' else GREETINGS_EN)
    # Identity
    if IDENTITY_PATTERNS.search(text):
        return IDENTITY_ES if lang == 'es' else IDENTITY_EN
    # FAQ lookup
    for faq in FAQ:
        for pattern in faq["_compiled"]:
            if pattern.search(text):
                return faq["es"] if lang == 'es' else faq["en"]
    return None


# ═════════════════════════════════════════════════════════════════════
# DAEMON RPC CLIENT (real-time network data)
# ═════════════════════════════════════════════════════════════════════

# Patterns that trigger live network status query
NETWORK_STATUS_PATTERNS = re.compile(
    r'(?:estado|status|stats?)\s*(?:de\s*)?(?:la\s*)?(?:red|network)|'
    r'(?:hash\s*rate|hashrate|poder\s*(?:de\s*)?(?:hash|c[oó]mputo)|potencia)|'
    r'(?:dificultad|difficulty)\s*(?:actual|current|ahora|now)?|'
    r'(?:altura|height|bloque\s*actual|current\s*block|last\s*block|[uú]ltimo\s*bloque)|'
    r'(?:c[oó]mo\s*(?:est[aá]|va)\s*(?:la\s*)?red)|'
    r'(?:how\s*(?:is|are)\s*(?:the\s*)?network)|'
    r'(?:network\s*(?:health|info|stats?|status))|'
    r'(?:peers?|nodos?|nodes?|conexiones|connections)',
    re.IGNORECASE
)


def format_hashrate(h: float) -> str:
    """Convert H/s to human-readable format."""
    if h >= 1e12:
        return f"{h/1e12:.2f} TH/s"
    elif h >= 1e9:
        return f"{h/1e9:.2f} GH/s"
    elif h >= 1e6:
        return f"{h/1e6:.2f} MH/s"
    elif h >= 1e3:
        return f"{h/1e3:.2f} KH/s"
    else:
        return f"{h:.0f} H/s"


def format_difficulty(d: int) -> str:
    """Convert difficulty to human-readable format."""
    if d >= 1e12:
        return f"{d/1e12:.2f}T"
    elif d >= 1e9:
        return f"{d/1e9:.2f}G"
    elif d >= 1e6:
        return f"{d/1e6:.2f}M"
    elif d >= 1e3:
        return f"{d/1e3:.2f}K"
    else:
        return str(d)


async def query_daemon_rpc(session: aiohttp.ClientSession) -> Optional[dict]:
    """Query ninacatcoind JSON RPC for network info.
    
    PRIVACY: Only returns public network data.
    NEVER returns transaction details, addresses, or sender/receiver info.
    """
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_info"
    }
    try:
        url = f"{DAEMON_RPC_URL}/json_rpc"
        async with session.post(url, json=payload, timeout=aiohttp.ClientTimeout(total=5)) as resp:
            if resp.status != 200:
                return None
            data = await resp.json()
            result = data.get("result", {})
            if not result:
                return None
            # Only extract PUBLIC network data — no tx details, no addresses
            return {
                "height": result.get("height", 0),
                "difficulty": result.get("difficulty", 0),
                "hash_rate": result.get("difficulty", 0) / 120,  # difficulty / block_time
                "incoming_connections": result.get("incoming_connections_count", 0),
                "outgoing_connections": result.get("outgoing_connections_count", 0),
                "status": result.get("status", "unknown"),
                "version": result.get("version", "unknown"),
                "synced": not result.get("busy_syncing", False),
                "top_block_hash": result.get("top_block_hash", "")[:16] + "..." if result.get("top_block_hash") else "N/A",
                "net_type": result.get("nettype", "mainnet"),
            }
    except Exception as e:
        logging.warning(f"[RPC] Daemon query failed: {e}")
        return None


async def build_network_status_embed(session: aiohttp.ClientSession, lang: str) -> Optional[discord.Embed]:
    """Build a Discord embed with real-time network stats from daemon."""
    info = await query_daemon_rpc(session)
    if not info:
        return None

    synced_emoji = "✅" if info["synced"] else "⏳"
    status_emoji = "✅" if info["status"] == "OK" else "⚠️"
    total_peers = info["incoming_connections"] + info["outgoing_connections"]

    if lang == 'es':
        title = "🐱 Estado de la Red NinaCatCoin"
        fields = [
            ("Altura", f"`{info['height']:,}`", True),
            ("Dificultad", f"`{format_difficulty(info['difficulty'])}`", True),
            ("Hash Rate", f"`{format_hashrate(info['hash_rate'])}`", True),
            ("Peers", f"`{total_peers}` (⬇{info['incoming_connections']} / ⬆{info['outgoing_connections']})", True),
            ("Estado", f"{status_emoji} {info['status']}", True),
            ("Sync", f"{synced_emoji} {'Sincronizado' if info['synced'] else 'Sincronizando...'}", True),
            ("Red", f"`{info['net_type']}`", True),
            ("Versión", f"`{info['version']}`", True),
        ]
        footer = "NINA AI — Datos en tiempo real del daemon"
    else:
        title = "🐱 NinaCatCoin Network Status"
        fields = [
            ("Height", f"`{info['height']:,}`", True),
            ("Difficulty", f"`{format_difficulty(info['difficulty'])}`", True),
            ("Hash Rate", f"`{format_hashrate(info['hash_rate'])}`", True),
            ("Peers", f"`{total_peers}` (⬇{info['incoming_connections']} / ⬆{info['outgoing_connections']})", True),
            ("Status", f"{status_emoji} {info['status']}", True),
            ("Sync", f"{synced_emoji} {'Synced' if info['synced'] else 'Syncing...'}", True),
            ("Network", f"`{info['net_type']}`", True),
            ("Version", f"`{info['version']}`", True),
        ]
        footer = "NINA AI — Live data from daemon"

    embed = discord.Embed(title=title, color=0x00ff88)
    for name, value, inline in fields:
        embed.add_field(name=name, value=value, inline=inline)
    embed.set_footer(text=footer)
    return embed


# ═══════════════════════════════════════════════════════════════════════
# LLM CLIENT (connects to llama-server HTTP API)
# ═══════════════════════════════════════════════════════════════════════

def trim_response(text: str) -> str:
    """Brutally enforce short responses from small model."""
    text = text.strip()
    if not text:
        return text
    
    # Remove everything after double newline
    idx = text.find('\n\n')
    if idx > 0:
        text = text[:idx].strip()
    
    # Cut at semicolons too (model uses run-on sentences)
    idx = text.find(';')
    if idx > 30:
        text = text[:idx].strip()
    
    # Cut at " - " dash separator (model restarts with "- NINA CATCOIN AI")
    idx = text.find(' - ')
    if idx > 30:
        text = text[:idx].strip()
    
    # Split into sentences and keep max 2
    sentences = re.split(r'(?<=[.!?])\s+', text)
    if len(sentences) > 2:
        text = ' '.join(sentences[:2]).strip()
    
    # Hard cap at 200 chars
    if len(text) > 200:
        cut = text[:200].rfind('.')
        if cut > 40:
            text = text[:cut+1]
        else:
            cut = text[:200].rfind(' ')
            text = text[:cut] + '.' if cut > 40 else text[:200] + '...'
    
    # Ensure ends with punctuation
    if text and text[-1] not in '.!?':
        text += '.'
    
    # Add cat emoji if not present
    if '\U0001f431' not in text and '\U0001f63a' not in text:
        text = '\U0001f431 ' + text
    
    return text


async def query_llm(user_message: str, session: aiohttp.ClientSession) -> str:
    """Send a message to llama-server and get NINA's response."""
    
    # Detect language and pick matching system prompt
    lang = detect_language(user_message)
    
    # Check for canned responses first (greetings, identity questions)
    canned = get_canned_response(user_message, lang)
    if canned:
        return canned
    
    system_prompt = get_system_prompt(lang)
    
    payload = {
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_message}
        ],
        "max_tokens": MAX_RESPONSE_TOKENS,
        "temperature": 0.1,
        "top_p": 0.5,
        "repeat_penalty": 1.8,
        "stop": ["\n\n", "<|eot_id|>", "\nUser:", "\nHuman:", "\nBy the way", "\nPor cierto", "\nAdemás", ";", " - "],
        "stream": False
    }
    
    try:
        url = f"{LLM_SERVER_URL}/v1/chat/completions"
        async with session.post(url, json=payload, timeout=aiohttp.ClientTimeout(total=30)) as resp:
            if resp.status != 200:
                error_text = await resp.text()
                logging.error(f"LLM server error {resp.status}: {error_text[:200]}")
                return "🐱 Mi red neuronal está procesando. ¡Intenta de nuevo en unos segundos!"
            
            data = await resp.json()
            
            if "choices" in data and len(data["choices"]) > 0:
                response = data["choices"][0].get("message", {}).get("content", "")
                if not response:
                    response = data["choices"][0].get("text", "")
                # Trim double-answers from small model
                response = trim_response(response)
                return response
            else:
                return "🐱 No pude generar una respuesta. ¡Intenta de nuevo!"
                
    except asyncio.TimeoutError:
        return "🐱 Respuesta agotada (>30s). El servidor puede estar ocupado. ¡Intenta de nuevo!"
    except aiohttp.ClientConnectorError:
        return "🐱 No puedo conectar con mi red neuronal (llama-server). ¿Está encendido?"
    except Exception as e:
        logging.error(f"LLM query error: {e}")
        return "🐱 Algo salió mal. ¡Intenta más tarde!"


# ═══════════════════════════════════════════════════════════════════════
# DISCORD BOT
# ═══════════════════════════════════════════════════════════════════════

# Rate limiting state
user_last_message: dict[int, float] = {}

intents = discord.Intents.default()
intents.message_content = True

bot = commands.Bot(command_prefix="!", intents=intents)
http_session: Optional[aiohttp.ClientSession] = None


@bot.event
async def on_ready():
    global http_session
    http_session = aiohttp.ClientSession()
    logging.info(f"[NINA BOT] Connected as {bot.user} (ID: {bot.user.id})")
    logging.info(f"[NINA BOT] LLM Server: {LLM_SERVER_URL}")
    logging.info(f"[NINA BOT] Daemon RPC: {DAEMON_RPC_URL}")
    logging.info(f"[NINA BOT] Rate limit: {RATE_LIMIT_SECONDS}s per user")
    logging.info(f"[NINA BOT] Servers: {len(bot.guilds)}")
    
    # Set status
    await bot.change_presence(
        activity=discord.Activity(
            type=discord.ActivityType.watching,
            name="NinaCatCoin network 🐱"
        )
    )


@bot.event
async def on_message(message: discord.Message):
    # Ignore own messages
    if message.author == bot.user:
        return
    
    # Ignore other bots
    if message.author.bot:
        return
    
    # Check if restricted to specific channel
    if ALLOWED_CHANNEL_ID and str(message.channel.id) != ALLOWED_CHANNEL_ID:
        # Only respond if bot is mentioned
        if not bot.user.mentioned_in(message):
            return
    
    # Check if bot was mentioned or message starts with "nina"
    is_for_nina = (
        bot.user.mentioned_in(message) or
        message.content.lower().startswith("nina ") or
        message.content.lower().startswith("nina,") or
        message.content.lower() == "nina"
    )
    
    if not is_for_nina:
        # Also respond in DMs
        if not isinstance(message.channel, discord.DMChannel):
            return
    
    # Clean the message (remove bot mention)
    user_text = message.content
    if bot.user:
        user_text = user_text.replace(f"<@{bot.user.id}>", "").replace(f"<@!{bot.user.id}>", "")
    user_text = user_text.strip()
    
    # Remove "nina" prefix if present
    if user_text.lower().startswith("nina"):
        user_text = user_text[4:].strip().lstrip(",").strip()
    
    if not user_text:
        await message.reply("🐱 ¡Hola! Soy NINA, la IA guardiana de NinaCatCoin. ¿En qué puedo ayudarte?")
        return
    
    # ─── Rate Limiting ───
    user_id = message.author.id
    now = time.time()
    last_time = user_last_message.get(user_id, 0)
    elapsed = now - last_time
    
    if elapsed < RATE_LIMIT_SECONDS:
        remaining = int(RATE_LIMIT_SECONDS - elapsed)
        await message.reply(f"🐱 Espera {remaining}s antes de enviar otro mensaje (rate limit: {RATE_LIMIT_SECONDS}s).")
        return
    
    # ─── Input Length Check ───
    if len(user_text) > MAX_INPUT_LENGTH:
        await message.reply(f"🐱 Mensaje muy largo ({len(user_text)} chars). Máximo: {MAX_INPUT_LENGTH}.")
        return
    
    # ─── Input Firewall ───
    block_reason = check_input_firewall(user_text)
    if block_reason:
        logging.warning(f"[FIREWALL] Blocked input from {message.author} ({message.author.id}): {block_reason}")
        await message.reply("🐱 No puedo ayudar con eso. Pregúntame sobre la red NinaCatCoin, minería o seguridad.")
        return
    
    # ─── Update rate limit timer ───
    user_last_message[user_id] = now
    
    # ─── Show typing indicator while generating ───
    async with message.channel.typing():
        # Check for live network status queries FIRST
        if NETWORK_STATUS_PATTERNS.search(user_text):
            embed = await build_network_status_embed(http_session, detect_language(user_text))
            if embed:
                await message.reply(embed=embed)
                logging.info(f"[RPC] {message.author}: network status query")
                return
            # Daemon offline — fall through to FAQ/LLM
        
        # Query LLM
        response = await query_llm(user_text, http_session)
        
        # Output Firewall
        response = sanitize_output(response)
        
        # Truncate if too long for Discord (2000 char limit)
        if len(response) > 1900:
            response = response[:1900] + "... 🐱 (truncated)"
    
    await message.reply(response)
    logging.info(f"[CHAT] {message.author}: {user_text[:80]}... → {len(response)} chars")


# ─── Slash command: /nina ───
@bot.command(name="nina")
async def nina_command(ctx, *, question: str = ""):
    """Chat with NINA AI. Usage: !nina <your question>"""
    if not question:
        await ctx.reply("🐱 Uso: `!nina ¿cómo está la red?`")
        return
    # Forward to on_message logic by creating a fake mention
    ctx.message.content = f"nina {question}"
    await on_message(ctx.message)


@bot.command(name="nina_status")
async def nina_status(ctx):
    """Check NINA bot and LLM server status."""
    try:
        async with http_session.get(f"{LLM_SERVER_URL}/health", timeout=aiohttp.ClientTimeout(total=5)) as resp:
            if resp.status == 200:
                llm_status = "✅ Online"
                health = await resp.json()
                model = health.get("model", "unknown")
            else:
                llm_status = f"⚠️ Status {resp.status}"
                model = "unknown"
    except:
        llm_status = "❌ Offline"
        model = "N/A"
    
    embed = discord.Embed(
        title="🐱 NINA AI Status",
        color=0x00ff88
    )
    embed.add_field(name="Bot", value="✅ Online", inline=True)
    embed.add_field(name="LLM Server", value=llm_status, inline=True)
    embed.add_field(name="Model", value=model, inline=True)
    embed.add_field(name="Rate Limit", value=f"{RATE_LIMIT_SECONDS}s/user", inline=True)
    embed.add_field(name="Max Tokens", value=str(MAX_RESPONSE_TOKENS), inline=True)
    embed.add_field(name="Firewall", value="✅ Active", inline=True)
    embed.set_footer(text="NinaCatCoin NINA AI Guardian")
    
    await ctx.reply(embed=embed)


@bot.command(name="nina_network")
async def nina_network(ctx):
    """Show real-time NinaCatCoin network stats from daemon RPC."""
    lang = detect_language(ctx.message.content)
    embed = await build_network_status_embed(http_session, lang)
    if embed:
        await ctx.reply(embed=embed)
    else:
        if lang == 'es':
            await ctx.reply("⚠️ No puedo conectar con el daemon. ¿Está `ninacatcoind` corriendo?")
        else:
            await ctx.reply("⚠️ Cannot connect to daemon. Is `ninacatcoind` running?")


@bot.event
async def on_close():
    if http_session:
        await http_session.close()


# ═══════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════

def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler("nina_bot.log", encoding="utf-8")
        ]
    )
    
    if not DISCORD_TOKEN:
        print("=" * 60)
        print("ERROR: Discord Bot Token not configured!")
        print()
        print("Option 1: Create .env file in this directory:")
        print("  NINA_DISCORD_TOKEN=your_token_here")
        print("  NINA_LLM_URL=http://127.0.0.1:8080")
        print()
        print("Option 2: Set environment variable:")
        print("  set NINA_DISCORD_TOKEN=your_token_here")
        print("=" * 60)
        sys.exit(1)
    
    logging.info("[NINA BOT] Starting NINA AI Discord Bot...")
    logging.info(f"[NINA BOT] LLM Server: {LLM_SERVER_URL}")
    logging.info(f"[NINA BOT] Daemon RPC: {DAEMON_RPC_URL}")
    logging.info(f"[NINA BOT] Firewall: {len(INPUT_BLOCKED_PATTERNS)} input patterns, {len(OUTPUT_BLOCKED_PATTERNS)} output patterns")
    
    bot.run(DISCORD_TOKEN)


if __name__ == "__main__":
    main()
