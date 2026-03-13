# NINA Discord Chat Bot

Optional tool to run NINA AI as a Discord bot in your community server.
NINA answers questions about NinaCatCoin, mining, privacy, and network health.

**This is NOT required for running a NinaCatCoin node** — the daemon already has NINA AI built-in.
This bot is for community admins who want NINA responding in their Discord channels.

## Requirements

- Python 3.9+
- A Discord Bot Token ([create one here](https://discord.com/developers/applications))
- `llama-server` running with a NINA GGUF model
- Optional: `ninacatcoind` running for live network stats

## Quick Start

### 1. Download the NINA model

```bash
# Download the fine-tuned NINA model from HuggingFace
# Q4_K_M (~1.9 GB for 3B, ~4.9 GB for 8B)
wget https://huggingface.co/ninacatcoin/nina-model/resolve/main/nina-3b-nina-ft-Q4_K_M.gguf
```

### 2. Start llama-server

**Linux / macOS:**
```bash
# Build llama-server (one time)
git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp && cmake -B build -DGGML_CUDA=OFF && cmake --build build --target llama-server -j$(nproc)

# Start server (CPU-only, good for VPS)
./build/bin/llama-server -m nina-3b-nina-ft-Q4_K_M.gguf --port 8080 -ngl 0 -t 8
```

**Windows:**
```powershell
# Download prebuilt binaries from https://github.com/ggerganov/llama.cpp/releases
llama-server.exe -m nina-3b-nina-ft-Q4_K_M.gguf --port 8080 -ngl 0 -t 8
```

### 3. Install Python dependencies

```bash
pip install discord.py aiohttp
```

### 4. Configure the bot

```bash
# Copy and edit the .env file
cp .env .env.backup
# Edit .env and set your NINA_DISCORD_TOKEN
```

**Important:** When creating your Discord bot, enable **MESSAGE CONTENT INTENT** in the Bot settings.

### 5. Start the bot

```bash
python nina_bot.py
```

## How Users Interact

| Method | Example |
|--------|---------|
| Mention the bot | `@NINA how is the network?` |
| Start with "nina" | `nina what is RandomX?` |
| Direct message | DM the bot directly |
| Command | `!nina what is NinaCatCoin?` |
| Network stats | `!nina_network` |
| Bot status | `!nina_status` |

Supports **English** and **Spanish** — NINA auto-detects the language.

## Security

NINA cannot modify the network, access wallets, or execute commands:

- **Input firewall**: blocks jailbreaks, financial commands, private key requests
- **Output firewall**: blocks addresses, keys, dangerous content in responses
- **Rate limit**: 1 message per 15 seconds per user (configurable)
- **Model is FROZEN** — never learns from Discord conversations
- **System prompt is hardcoded** — cannot be overridden by users
- **Daemon RPC is read-only** — only queries public network stats (height, difficulty, peers)
- **No wallet access** — NINA has no ability to send transactions or sign anything

## Configuration (.env)

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `NINA_DISCORD_TOKEN` | Yes | — | Your Discord bot token |
| `NINA_LLM_URL` | No | `http://127.0.0.1:8080` | llama-server endpoint |
| `NINA_CHANNEL_ID` | No | (all channels) | Restrict to one channel |
| `NINA_DAEMON_RPC` | No | `http://127.0.0.1:19021` | Daemon RPC for live stats |

## License

Copyright (c) 2026, The NinaCatCoin Project. Same license as the main project.
