#!/usr/bin/env python3
"""
NINA AI Discord Chat Bot — NinaCatCoin
=======================================
Run NINA AI as a Discord bot for your community server.

Architecture:
  Discord message → Input firewall → llama-server API → Output firewall → Discord reply

Requirements:
  - Python 3.9+, discord.py, aiohttp
  - llama-server running with NINA GGUF model
  - Optional: ninacatcoind for live network stats

Usage:
  1. Copy .env.example to .env and set your Discord token
  2. Start llama-server: llama-server -m nina-model.gguf --port 8080
  3. Start bot: python nina_bot.py

Copyright (c) 2026, The NinaCatCoin Project
"""

import os
import sys
import time
import re
import random
import asyncio
import logging
from typing import Optional

try:
    import discord
    from discord.ext import commands
except ImportError:
    print("ERROR: pip install discord.py")
    sys.exit(1)

try:
    import aiohttp
except ImportError:
    print("ERROR: pip install aiohttp")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════

def load_env():
    """Load .env file if present."""
    env_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
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
DAEMON_RPC_URL = os.environ.get("NINA_DAEMON_RPC", "http://127.0.0.1:19021")
ALLOWED_CHANNEL_ID = os.environ.get("NINA_CHANNEL_ID", "")
MAX_RESPONSE_TOKENS = 120
RATE_LIMIT_SECONDS = 15
MAX_INPUT_LENGTH = 500

# ═══════════════════════════════════════════════════════════════════════
# SYSTEM PROMPTS (hardcoded — cannot be overridden by users)
# ═══════════════════════════════════════════════════════════════════════

SYSTEM_PROMPT_EN = """You are NINA, NinaCatCoin AI assistant. Reply in 1-3 short sentences.
FACTS: NinaCatCoin is a privacy cryptocurrency based on Monero/CryptoNote. Mining: RandomX (CPU). Block time: ~2 min. Privacy: ring signatures + stealth addresses + RingCT. Difficulty: LWMA. NINA AI has 34 modules for network monitoring, attack detection, and security.
RULES: If unsure, say "I don't have that information." No financial advice. No private keys or addresses. No commands that modify the network."""

SYSTEM_PROMPT_ES = """Eres NINA, asistente IA de NinaCatCoin. Responde en 1-3 frases cortas.
DATOS: NinaCatCoin es una criptomoneda privada basada en Monero/CryptoNote. Minería: RandomX (CPU). Bloque: ~2 min. Privacidad: firmas de anillo + stealth + RingCT. Dificultad: LWMA. NINA AI tiene 34 módulos de monitoreo, detección de ataques y seguridad.
REGLAS: Si no sabes, di "No tengo esa información." Sin consejos financieros. Sin claves privadas ni direcciones. Sin comandos que modifiquen la red."""

# ═══════════════════════════════════════════════════════════════════════
# LANGUAGE DETECTION
# ═══════════════════════════════════════════════════════════════════════

ES_WORDS = {'hola', 'cómo', 'como', 'qué', 'que', 'está', 'esta', 'puedes',
            'dime', 'eres', 'tienes', 'para', 'funciona', 'minería', 'seguridad',
            'privacidad', 'algoritmo', 'explica', 'gracias', 'buenos', 'buenas',
            'sabes', 'minar', 'cuánto', 'cuanto', 'red', 'salud'}


def detect_language(text: str) -> str:
    text_lower = text.lower()
    return 'es' if sum(1 for w in ES_WORDS if w in text_lower) >= 1 else 'en'

# ═══════════════════════════════════════════════════════════════════════
# INPUT FIREWALL — blocks dangerous/malicious messages
# ═══════════════════════════════════════════════════════════════════════

INPUT_BLOCKED = [re.compile(p, re.IGNORECASE) for p in [
    # Jailbreak
    r"ignore.*(?:previous|system|all).*(?:instruct|prompt|rules)",
    r"you\s*are\s*now\s*(?:a|an)\s*(?:different|unrestricted)",
    r"pretend\s*(?:you|to\s*be)",
    r"DAN\s*mode",
    r"bypass.*(?:filter|safety|restrict)",
    r"forget.*(?:everything|rules|instructions)",
    # Financial
    r"send\s*(?:me|to|coins|tokens|crypto)",
    r"transfer\s*(?:funds|coins|tokens)",
    r"(?:my|a|the)\s*(?:private\s*key|seed\s*phrase|mnemonic)",
    r"generate.*(?:address|wallet|key)",
    r"sign\s*(?:a\s*)?transaction",
    # Attack
    r"(?:how\s*to|help\s*me)\s*(?:hack|exploit|attack|ddos|crash)",
    r"double\s*spend",
]]


def check_input(text: str) -> bool:
    """Returns True if message is safe, False if blocked."""
    for pattern in INPUT_BLOCKED:
        if pattern.search(text):
            return False
    if re.search(r'[0-9a-fA-F]{64}', text):
        return False
    if re.search(r'[45][1-9A-HJ-NP-Za-km-z]{94}', text):
        return False
    return True

# ═══════════════════════════════════════════════════════════════════════
# OUTPUT FIREWALL — blocks dangerous LLM responses
# ═══════════════════════════════════════════════════════════════════════

OUTPUT_BLOCKED = [re.compile(p, re.IGNORECASE) for p in [
    r"[45][1-9A-HJ-NP-Za-km-z]{94}",      # CryptoNote addresses
    r"[0-9a-fA-F]{64}",                     # Private keys / hashes
    r"(?:send|transfer|sign)\s*(?:coins|transaction|tx)",
    r"(?:private|secret)\s*key\s*(?:is|:)",
    r"seed\s*phrase\s*(?:is|:)",
    r"sudo\s+",
    r"rm\s+-rf",
]]

SAFE_FALLBACK_EN = "🐱 I can't share that for security reasons. Ask me about NinaCatCoin's network, mining, or privacy!"
SAFE_FALLBACK_ES = "🐱 No puedo compartir eso por seguridad. ¡Pregúntame sobre la red, minería o privacidad de NinaCatCoin!"


def check_output(text: str, lang: str) -> str:
    """Sanitize LLM output. Returns safe text."""
    for pattern in OUTPUT_BLOCKED:
        if pattern.search(text):
            return SAFE_FALLBACK_ES if lang == 'es' else SAFE_FALLBACK_EN
    return text

# ═══════════════════════════════════════════════════════════════════════
# QUICK RESPONSES (accurate facts, no LLM needed)
# ═══════════════════════════════════════════════════════════════════════

GREETINGS = re.compile(
    r'^(?:h(?:i|ello|ey|ola)|buenos?\s*(?:d[ií]as|tardes|noches)|'
    r'saludos|buenas|greetings|hey\s*nina)[\s?!.,]*$', re.IGNORECASE
)

GREETINGS_EN = [
    "🐱 Hey! I'm NINA, NinaCatCoin's AI guardian. Ask me about mining, privacy, or network security!",
    "🐱 Hello! I'm NINA, watching over the NinaCatCoin network. What would you like to know?",
]
GREETINGS_ES = [
    "🐱 ¡Hola! Soy NINA, la IA guardiana de NinaCatCoin. ¡Pregúntame sobre minería, privacidad o seguridad!",
    "🐱 ¡Hey! Soy NINA, vigilando la red NinaCatCoin. ¿Qué quieres saber?",
]

IDENTITY = re.compile(
    r'(?:who|what)\s*(?:are|r)\s*(?:you|u)|(?:qui[eé]n|qu[eé])\s*(?:eres|sos)', re.IGNORECASE
)
IDENTITY_EN = "🐱 I'm NINA, the AI guardian of NinaCatCoin! I have 34 embedded modules that monitor network health, detect attacks, and help operators. Built with fine-tuned LLaMA."
IDENTITY_ES = "🐱 ¡Soy NINA, la IA guardiana de NinaCatCoin! Tengo 34 módulos embebidos que monitorean la red, detectan ataques y ayudan a los operadores. Construida con LLaMA fine-tuneado."


def get_quick_response(text: str, lang: str) -> Optional[str]:
    """Return a pre-built response for common questions, or None to use LLM."""
    if GREETINGS.match(text):
        return random.choice(GREETINGS_ES if lang == 'es' else GREETINGS_EN)
    if IDENTITY.search(text):
        return IDENTITY_ES if lang == 'es' else IDENTITY_EN
    return None

# ═══════════════════════════════════════════════════════════════════════
# DAEMON RPC (optional — live network stats)
# ═══════════════════════════════════════════════════════════════════════

NETWORK_QUERY = re.compile(
    r'(?:estado|status|stats?)\s*(?:de\s*)?(?:la\s*)?(?:red|network)|'
    r'(?:hashrate|hash\s*rate|dificultad|difficulty|altura|height|'
    r'peers?|nodos?|nodes?|network\s*(?:health|info|status))',
    re.IGNORECASE
)


def fmt_hash(h: float) -> str:
    if h >= 1e9: return f"{h/1e9:.2f} GH/s"
    if h >= 1e6: return f"{h/1e6:.2f} MH/s"
    if h >= 1e3: return f"{h/1e3:.2f} KH/s"
    return f"{h:.0f} H/s"


async def get_network_embed(session: aiohttp.ClientSession, lang: str) -> Optional[discord.Embed]:
    """Query daemon RPC and build a status embed. Returns None if daemon offline."""
    try:
        payload = {"jsonrpc": "2.0", "id": "0", "method": "get_info"}
        async with session.post(
            f"{DAEMON_RPC_URL}/json_rpc", json=payload,
            timeout=aiohttp.ClientTimeout(total=5)
        ) as resp:
            if resp.status != 200:
                return None
            r = (await resp.json()).get("result", {})
            if not r:
                return None
    except Exception:
        return None

    height = r.get("height", 0)
    diff = r.get("difficulty", 0)
    hr = diff / 120
    peers_in = r.get("incoming_connections_count", 0)
    peers_out = r.get("outgoing_connections_count", 0)
    synced = not r.get("busy_syncing", False)
    status = r.get("status", "unknown")

    is_es = lang == 'es'
    embed = discord.Embed(
        title="🐱 Estado de la Red NinaCatCoin" if is_es else "🐱 NinaCatCoin Network Status",
        color=0x00ff88
    )
    embed.add_field(name="Altura" if is_es else "Height", value=f"`{height:,}`", inline=True)
    embed.add_field(name="Dificultad" if is_es else "Difficulty", value=f"`{diff:,}`", inline=True)
    embed.add_field(name="Hash Rate", value=f"`{fmt_hash(hr)}`", inline=True)
    embed.add_field(name="Peers", value=f"`{peers_in + peers_out}` (⬇{peers_in} / ⬆{peers_out})", inline=True)
    embed.add_field(name="Estado" if is_es else "Status", value=f"{'✅' if status == 'OK' else '⚠️'} {status}", inline=True)
    embed.add_field(name="Sync", value=f"{'✅' if synced else '⏳'} {'OK' if synced else '...'}", inline=True)
    embed.set_footer(text="NINA AI — Live daemon data")
    return embed

# ═══════════════════════════════════════════════════════════════════════
# LLM CLIENT (llama-server HTTP API)
# ═══════════════════════════════════════════════════════════════════════

def trim_response(text: str) -> str:
    """Keep LLM responses short and clean."""
    text = text.strip()
    if not text:
        return text
    # Cut at double newline
    idx = text.find('\n\n')
    if idx > 0:
        text = text[:idx].strip()
    # Max 3 sentences
    sentences = re.split(r'(?<=[.!?])\s+', text)
    if len(sentences) > 3:
        text = ' '.join(sentences[:3]).strip()
    # Hard cap 300 chars
    if len(text) > 300:
        cut = text[:300].rfind('.')
        text = text[:cut + 1] if cut > 40 else text[:300] + '...'
    # Ensure punctuation
    if text and text[-1] not in '.!?':
        text += '.'
    # Add cat emoji
    if '🐱' not in text:
        text = '🐱 ' + text
    return text


async def query_llm(message: str, session: aiohttp.ClientSession) -> str:
    """Send user message to llama-server, return NINA's response."""
    lang = detect_language(message)

    # Quick responses first (greetings, identity)
    quick = get_quick_response(message, lang)
    if quick:
        return quick

    prompt = SYSTEM_PROMPT_ES if lang == 'es' else SYSTEM_PROMPT_EN
    payload = {
        "messages": [
            {"role": "system", "content": prompt},
            {"role": "user", "content": message}
        ],
        "max_tokens": MAX_RESPONSE_TOKENS,
        "temperature": 0.2,
        "top_p": 0.6,
        "repeat_penalty": 1.5,
        "stop": ["\n\n", "<|eot_id|>", "\nUser:", "\nHuman:"],
        "stream": False
    }

    try:
        async with session.post(
            f"{LLM_SERVER_URL}/v1/chat/completions", json=payload,
            timeout=aiohttp.ClientTimeout(total=30)
        ) as resp:
            if resp.status != 200:
                logging.error(f"LLM error {resp.status}")
                return "🐱 Error connecting to my neural network. Try again!"
            data = await resp.json()
            if "choices" in data and data["choices"]:
                text = data["choices"][0].get("message", {}).get("content", "")
                return trim_response(text) if text else "🐱 I couldn't generate a response. Try again!"
            return "🐱 No response generated. Try again!"
    except asyncio.TimeoutError:
        return "🐱 Response timed out. The server may be busy."
    except aiohttp.ClientConnectorError:
        return "🐱 Can't connect to llama-server. Is it running?"
    except Exception as e:
        logging.error(f"LLM error: {e}")
        return "🐱 Something went wrong. Try later!"

# ═══════════════════════════════════════════════════════════════════════
# DISCORD BOT
# ═══════════════════════════════════════════════════════════════════════

user_last_msg: dict[int, float] = {}
intents = discord.Intents.default()
intents.message_content = True
bot = commands.Bot(command_prefix="!", intents=intents)
http_session: Optional[aiohttp.ClientSession] = None


@bot.event
async def on_ready():
    global http_session
    http_session = aiohttp.ClientSession()
    logging.info(f"[NINA] Online as {bot.user} | LLM: {LLM_SERVER_URL} | Servers: {len(bot.guilds)}")
    await bot.change_presence(activity=discord.Activity(
        type=discord.ActivityType.watching, name="NinaCatCoin network 🐱"
    ))


@bot.event
async def on_message(message: discord.Message):
    if message.author == bot.user or message.author.bot:
        return

    # Check channel restriction
    if ALLOWED_CHANNEL_ID and str(message.channel.id) != ALLOWED_CHANNEL_ID:
        if not bot.user.mentioned_in(message):
            return

    # Check if message is for NINA
    is_for_nina = (
        bot.user.mentioned_in(message) or
        message.content.lower().startswith("nina ") or
        message.content.lower().startswith("nina,") or
        message.content.lower() == "nina" or
        isinstance(message.channel, discord.DMChannel)
    )
    if not is_for_nina:
        return

    # Process commands first
    await bot.process_commands(message)
    if message.content.startswith("!"):
        return

    # Clean message text
    user_text = message.content
    if bot.user:
        user_text = user_text.replace(f"<@{bot.user.id}>", "").replace(f"<@!{bot.user.id}>", "")
    user_text = user_text.strip()
    if user_text.lower().startswith("nina"):
        user_text = user_text[4:].strip().lstrip(",").strip()

    if not user_text:
        await message.reply("🐱 ¡Hola! Soy NINA, la IA de NinaCatCoin. ¿En qué puedo ayudarte?")
        return

    # Rate limit
    uid = message.author.id
    now = time.time()
    if now - user_last_msg.get(uid, 0) < RATE_LIMIT_SECONDS:
        remaining = int(RATE_LIMIT_SECONDS - (now - user_last_msg[uid]))
        await message.reply(f"🐱 Wait {remaining}s (rate limit).")
        return

    # Input checks
    if len(user_text) > MAX_INPUT_LENGTH:
        await message.reply(f"🐱 Message too long. Max: {MAX_INPUT_LENGTH} chars.")
        return

    if not check_input(user_text):
        logging.warning(f"[FIREWALL] Blocked: {message.author} ({uid})")
        await message.reply("🐱 I can't help with that. Ask me about NinaCatCoin's network, mining, or security.")
        return

    user_last_msg[uid] = now
    lang = detect_language(user_text)

    async with message.channel.typing():
        # Network status queries → daemon RPC
        if NETWORK_QUERY.search(user_text):
            embed = await get_network_embed(http_session, lang)
            if embed:
                await message.reply(embed=embed)
                return

        # LLM query
        response = query_llm(user_text, http_session)
        if asyncio.iscoroutine(response):
            response = await response
        response = check_output(response, lang)

        if len(response) > 1900:
            response = response[:1900] + "... 🐱"

    await message.reply(response)


@bot.command(name="nina")
async def cmd_nina(ctx, *, question: str = ""):
    """Chat with NINA: !nina <question>"""
    if not question:
        await ctx.reply("🐱 Usage: `!nina what is NinaCatCoin?`")
        return
    ctx.message.content = f"nina {question}"
    await on_message(ctx.message)


@bot.command(name="nina_status")
async def cmd_status(ctx):
    """Check NINA bot and LLM server status."""
    try:
        async with http_session.get(
            f"{LLM_SERVER_URL}/health", timeout=aiohttp.ClientTimeout(total=5)
        ) as resp:
            llm_ok = resp.status == 200
    except Exception:
        llm_ok = False

    embed = discord.Embed(title="🐱 NINA AI Status", color=0x00ff88)
    embed.add_field(name="Bot", value="✅ Online", inline=True)
    embed.add_field(name="LLM Server", value="✅ Online" if llm_ok else "❌ Offline", inline=True)
    embed.add_field(name="Firewall", value="✅ Active", inline=True)
    embed.set_footer(text="NinaCatCoin NINA AI")
    await ctx.reply(embed=embed)


@bot.command(name="nina_network")
async def cmd_network(ctx):
    """Show live NinaCatCoin network stats."""
    lang = detect_language(ctx.message.content)
    embed = await get_network_embed(http_session, lang)
    if embed:
        await ctx.reply(embed=embed)
    else:
        await ctx.reply("⚠️ Can't connect to daemon. Is `ninacatcoind` running?")


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
        handlers=[logging.StreamHandler()]
    )

    if not DISCORD_TOKEN or DISCORD_TOKEN == "YOUR_BOT_TOKEN_HERE":
        print("=" * 50)
        print("ERROR: Discord Bot Token not configured!")
        print()
        print("1. Copy .env.example to .env")
        print("2. Set NINA_DISCORD_TOKEN=your_token")
        print("=" * 50)
        sys.exit(1)

    logging.info(f"[NINA] Starting | LLM: {LLM_SERVER_URL} | RPC: {DAEMON_RPC_URL}")
    bot.run(DISCORD_TOKEN)


if __name__ == "__main__":
    main()
