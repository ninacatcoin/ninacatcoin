#!/bin/bash
# ============================================================================
# NINA AI Model Downloader
# Downloads LLaMA 3.2 3B (Q4_K_M quantized) for NINA AI intelligence
#
# Copyright (c) 2026, The NinaCatCoin Project
# ============================================================================

set -e

MODEL_DIR="$HOME/.ninacatcoin/models"
MODEL_FILE="Llama-3.2-3B-Instruct-Q4_K_M.gguf"
MODEL_PATH="$MODEL_DIR/$MODEL_FILE"

# Hugging Face URL for LLaMA 3.2 3B Instruct GGUF (Q4_K_M quantization)
MODEL_URL="https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"
MODEL_SIZE_MB=2000
MODEL_SHA256=""  # Will be filled when we verify the download

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           NINA AI — Model Downloader                       ║"
echo "║           LLaMA 3.2 3B Instruct (Q4_K_M)                  ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo "║  Model: LLaMA 3.2 3B Instruct                             ║"
echo "║  Quantization: Q4_K_M (4-bit, optimized)                  ║"
echo "║  Size: ~${MODEL_SIZE_MB}MB                                       ║"
echo "║  RAM Usage: ~2.5GB when loaded                             ║"
echo "║  GPU: Not required (CPU inference)                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if already downloaded
if [ -f "$MODEL_PATH" ]; then
    FILE_SIZE=$(stat -f%z "$MODEL_PATH" 2>/dev/null || stat -c%s "$MODEL_PATH" 2>/dev/null || echo "0")
    FILE_SIZE_MB=$((FILE_SIZE / 1024 / 1024))
    echo "✓ Model already exists: $MODEL_PATH ($FILE_SIZE_MB MB)"
    echo ""
    read -p "Re-download? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Keeping existing model."
        exit 0
    fi
fi

# Create directory
mkdir -p "$MODEL_DIR"
echo "📁 Download directory: $MODEL_DIR"
echo ""

# Download
echo "⚓️  Downloading LLaMA 3.2 3B (~${MODEL_SIZE_MB}MB)..."
echo "   From: $MODEL_URL"
echo ""

if command -v wget &> /dev/null; then
    wget --progress=bar:force -O "$MODEL_PATH" "$MODEL_URL"
elif command -v curl &> /dev/null; then
    curl -L --progress-bar -o "$MODEL_PATH" "$MODEL_URL"
else
    echo "ERROR: Neither wget nor curl found. Please install one of them."
    exit 1
fi

# Verify download
if [ -f "$MODEL_PATH" ]; then
    FILE_SIZE=$(stat -f%z "$MODEL_PATH" 2>/dev/null || stat -c%s "$MODEL_PATH" 2>/dev/null || echo "0")
    FILE_SIZE_MB=$((FILE_SIZE / 1024 / 1024))
    
    if [ "$FILE_SIZE_MB" -lt 100 ]; then
        echo "❌ ERROR: Downloaded file is too small ($FILE_SIZE_MB MB). Download may have failed."
        rm -f "$MODEL_PATH"
        exit 1
    fi
    
    echo ""
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║  ✅ NINA AI Model downloaded successfully!                 ║"
    echo "║                                                            ║"
    echo "║  File: $MODEL_PATH"
    echo "║  Size: ${FILE_SIZE_MB}MB"
    echo "║                                                            ║"
    echo "║  Start ninacatcoind normally — NINA will detect the model  ║"
    echo "║  and enable LLM intelligence automatically.                ║"
    echo "║                                                            ║"
    echo "║  Config options in ninacatcoin.conf:                       ║"
    echo "║    nina-llm-mode=active  (default, 2.5GB RAM, instant)    ║"
    echo "║    nina-llm-mode=lazy    (load on demand, 0MB idle)        ║"
    echo "║    nina-llm-mode=disabled (no LLM)                        ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
else
    echo "❌ ERROR: Download failed. File not found."
    exit 1
fi
