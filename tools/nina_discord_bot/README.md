# NINA Discord Chat Bot
# =====================
#
# Connects NINA AI (LLaMA 3.2 3B) to Discord for community chat.
#
# ## Quick Start
#
# 1. Start llama-server (CPU-only, simulating VPS):
#    ```
#    cd I:\ninacatcoin\external\llama.cpp\build\bin\Release
#    llama-server.exe -m path/to/Llama-3.2-3B-Instruct-Q4_K_M.gguf --port 8080 -ngl 0 -t 8
#    ```
#
# 2. Install Python dependencies:
#    ```
#    pip install discord.py aiohttp
#    ```
#
# 3. Configure .env file with your Discord Bot Token
#
# 4. Start bot:
#    ```
#    cd I:\ninacatcoin\tools\nina_discord_bot
#    python nina_bot.py
#    ```
#
# ## How Users Interact
#
# - Mention the bot: `@NINA how is the network?`
# - Start with "nina": `nina what is RandomX?`
# - Direct message the bot
# - Command: `!nina what is NinaCatCoin?`
# - Status: `!nina_status`
#
# ## Security
#
# - Input firewall: blocks jailbreaks, financial commands, key requests
# - Output firewall: blocks addresses, keys, dangerous content
# - Rate limit: 1 message per 15 seconds per user
# - Model is FROZEN — never learns from conversations
# - System prompt is hardcoded in Python — cannot be overridden
#
# ## VPS Deployment
#
# Same setup on VPS (Ubuntu):
# ```bash
# # Build llama-server
# cd /opt/ninacatcoin/external/llama.cpp
# cmake -B build -DGGML_CUDA=OFF
# cmake --build build --target llama-server -j$(nproc)
#
# # Download model
# wget https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf
#
# # Start server (background)
# ./build/bin/llama-server -m Llama-3.2-3B-Instruct-Q4_K_M.gguf --port 8080 -t 8 &
#
# # Start bot
# pip3 install discord.py aiohttp
# python3 nina_bot.py &
# ```
