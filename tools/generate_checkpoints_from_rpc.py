#!/usr/bin/env python3
"""
Generador de checkpoints que crea JSON a partir de RPC
Genera checkpoints para distribución en https://ninacatcoin.es/checkpoints/checkpoints.json
"""

import requests
import json
import sys
from pathlib import Path
from datetime import datetime

def get_blockchain_height(rpc_url):
    """Obtener altura actual de la blockchain."""
    try:
        response = requests.post(
            f"{rpc_url}/json_rpc",
            json={
                "jsonrpc": "2.0",
                "id": "0",
                "method": "get_info"
            },
            timeout=10
        )
        if response.status_code == 200:
            result = response.json()
            if 'result' in result:
                return result['result']['height']
    except Exception as e:
        print(f"ERROR getting blockchain height: {e}")
    return None

def get_block_hash_from_rpc(rpc_url, height):
    """Obtener hash del bloque desde RPC."""
    try:
        response = requests.post(
            f"{rpc_url}/json_rpc",
            json={
                "jsonrpc": "2.0",
                "id": "0",
                "method": "get_block_header_by_height",
                "params": {"height": height}
            },
            timeout=10
        )
        if response.status_code == 200:
            result = response.json()
            if 'result' in result:
                return result['result']['block_header']['hash']
    except Exception as e:
        print(f"ERROR getting block {height}: {e}")
    return None

def generate_checkpoints_json(rpc_url, output_file, checkpoint_interval=50):
    """Generar archivo JSON de checkpoints automáticamente."""
    
    # Obtener altura actual
    current_height = get_blockchain_height(rpc_url)
    if current_height is None:
        print(f"ERROR: Could not connect to RPC at {rpc_url}")
        return False
    
    print(f"[{datetime.now().isoformat()}] Generating checkpoints from RPC: {rpc_url}")
    print(f"Current blockchain height: {current_height}")
    
    checkpoints = []
    
    # Generar checkpoints cada checkpoint_interval bloques
    for height in range(0, current_height, checkpoint_interval):
        hash_val = get_block_hash_from_rpc(rpc_url, height)
        if hash_val:
            checkpoints.append({
                "height": height,
                "hash": hash_val
            })
            print(f"  ✓ Height {height}: {hash_val[:16]}...")
        else:
            print(f"  ✗ Failed to get hash for height {height}")
    
    # Siempre incluir el bloque actual (más reciente)
    if current_height > 0 and current_height % checkpoint_interval != 0:
        hash_val = get_block_hash_from_rpc(rpc_url, current_height)
        if hash_val:
            checkpoints.append({
                "height": current_height,
                "hash": hash_val
            })
            print(f"  ✓ Height {current_height} (latest): {hash_val[:16]}...")
    
    # Crear estructura JSON compatible con ninacatcoin
    data = {
        "hashlines": checkpoints
    }
    
    # Guardar JSON
    Path(output_file).parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"\n✓ Checkpoints saved to: {output_file}")
    print(f"  Total checkpoints: {len(checkpoints)}")
    return True

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate checkpoints JSON from RPC")
    parser.add_argument("--rpc-url", required=True, help="RPC endpoint URL")
    parser.add_argument("--heights", default="0,100,1000", help="Comma-separated block heights (DEPRECATED - now auto-generates)")
    parser.add_argument("--output-json", required=True, help="Output JSON file path")
    parser.add_argument("--interval", type=int, default=50, help="Checkpoint interval (default: 50 blocks)")
    
    args = parser.parse_args()
    
    generate_checkpoints_json(args.rpc_url, args.output_json, args.interval)
