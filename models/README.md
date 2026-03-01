# NINA AI Model Directory

This directory is for the NINA GGUF model file used by the ninacatcoin daemon.

## Current Model

**Status**: Training in progress (Llama 3.2 3B Instruct + QLoRA, 3,074 pairs)

## Deployment Steps

After training completes:

1. **Convert to GGUF Q4_K_M**:
   ```bash
   python merge_and_convert.py
   ```

2. **Compute SHA-256**:
   ```bash
   sha256sum nina_model.gguf
   ```

3. **Update compiled hash** in `src/cryptonote_config.h`:
   ```cpp
   #define NINA_MODEL_HASH "your_64_char_hex_hash_here"
   ```

4. **Place model** in the ninacatcoin data directory:
   - Linux: `~/.ninacatcoin/nina_model.gguf`
   - Windows: `%APPDATA%\ninacatcoin\nina_model.gguf`

5. **For llama-server** (VPS advisory mode):
   ```bash
   llama-server -m nina_model.gguf --host 127.0.0.1 --port 8080 -ngl 0 -c 2048
   ```

## Model Specifications

| Property | Value |
|----------|-------|
| Base model | Llama 3.2 3B Instruct |
| Fine-tune | QLoRA (r=64, alpha=128) |
| Quantization | Q4_K_M |
| Expected size | ~1.8 GB |
| Training data | 3,074 pairs (~317K tokens) |
| Filename | nina_model.gguf |

## Verification

The daemon automatically verifies the model at startup:
- Computes SHA-256 of `nina_model.gguf`
- Compares against `NINA_MODEL_HASH` compiled in the binary
- If mismatch → model rejected, NINA runs in stub mode
- If valid → hash stored for coinbase tag 0xCA embedding

## Files

- `nina_model.gguf` — Place the trained model here (not tracked in git)
