#!/usr/bin/env python3
"""
extract_training_data.py — Extract REAL blockchain data from ninacatcoin daemon
for ML model training.

Connects to the daemon RPC (default: http://127.0.0.1:19081) and fetches
all block headers, computes ML features, and writes to CSV.

Usage:
    python extract_training_data.py [--host HOST] [--port PORT] [--output FILE]

Requirements:
    - ninacatcoind running with RPC enabled
    - requests library (pip install requests)

Output CSV columns (matching train_models.py v2.0 expectations):
    height, timestamp, solve_time, difficulty, cumulative_difficulty,
    txs_count, block_size_bytes, network_health, miner_diversity,
    hash_entropy, block_accepted
"""

import argparse
import csv
import json
import sys
import time
import math
import requests
from pathlib import Path
from collections import deque


# =========================================================
# RPC Client
# =========================================================

class NinacatcoinRPC:
    """Simple JSON-RPC client for ninacatcoind."""
    
    def __init__(self, host='127.0.0.1', port=19081):
        self.url = f'http://{host}:{port}/json_rpc'
        self.session = requests.Session()
        self.session.headers.update({'Content-Type': 'application/json'})
    
    def _call(self, method, params=None):
        payload = {
            'jsonrpc': '2.0',
            'id': '0',
            'method': method,
        }
        if params:
            payload['params'] = params
        
        try:
            resp = self.session.post(self.url, json=payload, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            if 'error' in data:
                raise RuntimeError(f"RPC error: {data['error']}")
            return data.get('result', {})
        except requests.exceptions.ConnectionError:
            print(f"\n✗ ERROR: Cannot connect to daemon at {self.url}")
            print("  Make sure ninacatcoind is running with RPC enabled:")
            print("  ./ninacatcoind --rpc-bind-port=19081")
            sys.exit(1)
    
    def get_info(self):
        return self._call('get_info')
    
    def get_block_headers_range(self, start_height, end_height):
        return self._call('getblockheadersrange', {
            'start_height': start_height,
            'end_height': end_height,
        })
    
    def get_block(self, height):
        return self._call('getblock', {'height': height})


# =========================================================
# Feature Computation
# =========================================================

def compute_hash_entropy(hash_hex: str) -> int:
    """
    Compute bit transitions in a block hash.
    Real entropy measurement — counts how many times consecutive bits differ.
    """
    if not hash_hex or len(hash_hex) < 8:
        return 0
    
    try:
        # Convert hex to binary string
        hash_int = int(hash_hex, 16)
        bit_length = len(hash_hex) * 4  # each hex char = 4 bits
        binary = format(hash_int, f'0{bit_length}b')
        
        # Count transitions (0→1 or 1→0)
        transitions = sum(1 for i in range(1, len(binary)) if binary[i] != binary[i-1])
        return transitions
    except (ValueError, OverflowError):
        return 128  # default for 256-bit hash


def compute_network_health(recent_solve_times, target_time=120.0, window=60):
    """
    Compute network health as the fraction of recent blocks
    that solved within a reasonable time range (0.25x to 4x target).
    """
    if not recent_solve_times:
        return 0.95  # default
    
    times = list(recent_solve_times)[-window:]
    healthy = sum(1 for t in times if target_time * 0.25 <= t <= target_time * 4.0)
    return healthy / len(times) if times else 0.95


def compute_miner_diversity(recent_difficulties, window=60):
    """
    Compute miner diversity proxy from difficulty variance.
    High variance in difficulty → many different miners with different hashrates.
    Low variance → monolithic mining (possibly one miner).
    Returns normalized value 0..1.
    """
    if len(recent_difficulties) < 2:
        return 0.5
    
    diffs = list(recent_difficulties)[-window:]
    mean_d = sum(diffs) / len(diffs)
    if mean_d < 1:
        return 0.5
    
    variance = sum((d - mean_d) ** 2 for d in diffs) / (len(diffs) - 1)
    stddev = math.sqrt(variance)
    cv = stddev / mean_d  # coefficient of variation
    
    # Normalize: cv of 0 → 0, cv of 0.5+ → 1
    return min(1.0, cv * 2.0)


# =========================================================
# Main Extraction
# =========================================================

def extract_all_blocks(rpc: NinacatcoinRPC, total_blocks: int, batch_size: int = 500):
    """
    Fetch all block headers from daemon in batches.
    Returns list of block_header dicts.
    """
    all_headers = []
    
    for start in range(0, total_blocks, batch_size):
        end = min(start + batch_size - 1, total_blocks - 1)
        
        pct = (start / total_blocks) * 100
        print(f"\r  Fetching blocks {start}-{end} ({pct:.0f}%)...", end='', flush=True)
        
        try:
            result = rpc.get_block_headers_range(start, end)
            headers = result.get('headers', [])
            all_headers.extend(headers)
        except Exception as e:
            print(f"\n  ⚠ Error fetching range {start}-{end}: {e}")
            # Try one by one as fallback
            for h in range(start, end + 1):
                try:
                    result = rpc.get_block_headers_range(h, h)
                    headers = result.get('headers', [])
                    all_headers.extend(headers)
                except:
                    print(f"\n  ✗ Skipping block {h}")
    
    print(f"\r  ✓ Fetched {len(all_headers)} block headers                    ")
    return all_headers


def headers_to_csv(headers: list, output_path: str, target_time: float = 120.0):
    """
    Convert block headers to training CSV with computed ML features.
    """
    if not headers:
        print("✗ No headers to process")
        return
    
    # Sort by height
    headers.sort(key=lambda h: h['height'])
    
    # Rolling windows for feature computation
    solve_times = deque(maxlen=60)
    difficulties = deque(maxlen=60)
    
    rows = []
    prev_timestamp = None
    
    for i, hdr in enumerate(headers):
        height = hdr['height']
        timestamp = hdr['timestamp']
        difficulty = hdr['difficulty']
        cumulative_difficulty = hdr['cumulative_difficulty']
        num_txes = hdr.get('num_txes', 0)
        block_size = hdr.get('block_size', 0)
        block_hash = hdr.get('hash', '')
        orphan = hdr.get('orphan_status', False)
        
        # Compute solve time
        if prev_timestamp is not None and timestamp > prev_timestamp:
            solve_time = timestamp - prev_timestamp
        elif height == 0:
            solve_time = target_time  # genesis
        else:
            solve_time = target_time  # fallback
        
        prev_timestamp = timestamp
        
        # Clamp extreme solve times (data cleaning)
        # Some blocks may have timestamps far apart due to network issues
        if solve_time > 3600:  # > 1 hour
            solve_time = 3600
        if solve_time < 1:
            solve_time = 1
        
        # Update rolling windows
        solve_times.append(solve_time)
        difficulties.append(difficulty)
        
        # Compute ML features
        network_health = compute_network_health(solve_times, target_time)
        miner_diversity = compute_miner_diversity(difficulties)
        hash_entropy = compute_hash_entropy(block_hash)
        
        # All blocks from the real blockchain are accepted (they passed consensus)
        block_accepted = 1
        
        rows.append({
            'height': height,
            'timestamp': timestamp,
            'solve_time': round(solve_time, 2),
            'difficulty': difficulty,
            'cumulative_difficulty': cumulative_difficulty,
            'txs_count': num_txes,
            'block_size_bytes': block_size,
            'network_health': round(network_health, 4),
            'miner_diversity': round(miner_diversity, 4),
            'hash_entropy': hash_entropy,
            'block_accepted': block_accepted,
        })
        
        if (i + 1) % 1000 == 0:
            print(f"\r  Processing features: {i+1}/{len(headers)}...", end='', flush=True)
    
    print(f"\r  ✓ Computed features for {len(rows)} blocks                    ")
    
    # Write CSV
    fieldnames = [
        'height', 'timestamp', 'solve_time', 'difficulty', 'cumulative_difficulty',
        'txs_count', 'block_size_bytes', 'network_health', 'miner_diversity',
        'hash_entropy', 'block_accepted'
    ]
    
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    
    print(f"  ✓ Written to {output} ({len(rows)} rows, {output.stat().st_size / 1024:.1f} KB)")
    
    # Print data summary
    if rows:
        solve_times_all = [r['solve_time'] for r in rows[1:]]  # skip genesis
        diffs_all = [r['difficulty'] for r in rows]
        
        print(f"\n  ══ Data Summary ══")
        print(f"  Blocks:     {rows[0]['height']} → {rows[-1]['height']}")
        print(f"  Time range: {rows[-1]['timestamp'] - rows[0]['timestamp']:.0f}s "
              f"({(rows[-1]['timestamp'] - rows[0]['timestamp']) / 86400:.1f} days)")
        
        if solve_times_all:
            avg_time = sum(solve_times_all) / len(solve_times_all)
            print(f"  Avg solve time: {avg_time:.1f}s (target: {target_time}s)")
        
        if diffs_all:
            print(f"  Difficulty:  min={min(diffs_all)}, max={max(diffs_all)}, "
                  f"avg={sum(diffs_all)/len(diffs_all):.0f}")
        
        health_vals = [r['network_health'] for r in rows]
        print(f"  Health:      min={min(health_vals):.2f}, max={max(health_vals):.2f}, "
              f"avg={sum(health_vals)/len(health_vals):.3f}")
        
        diversity_vals = [r['miner_diversity'] for r in rows]
        print(f"  Diversity:   min={min(diversity_vals):.2f}, max={max(diversity_vals):.2f}, "
              f"avg={sum(diversity_vals)/len(diversity_vals):.3f}")
        
        entropy_vals = [r['hash_entropy'] for r in rows]
        print(f"  Entropy:     min={min(entropy_vals)}, max={max(entropy_vals)}, "
              f"avg={sum(entropy_vals)/len(entropy_vals):.0f}")


def main():
    parser = argparse.ArgumentParser(
        description='Extract ninacatcoin blockchain data for ML training'
    )
    parser.add_argument('--host', default='127.0.0.1', help='Daemon RPC host')
    parser.add_argument('--port', type=int, default=19081, help='Daemon RPC port')
    parser.add_argument('--output', default=None,
                        help='Output CSV path (default: ~/.ninacatcoin/ml_training_data.csv)')
    parser.add_argument('--batch-size', type=int, default=500,
                        help='Blocks per RPC request (max ~1000)')
    
    args = parser.parse_args()
    
    # Default output path
    if args.output is None:
        import os
        if os.name == 'nt':  # Windows
            data_dir = Path(os.environ.get('USERPROFILE', '~')) / '.ninacatcoin'
        else:
            data_dir = Path.home() / '.ninacatcoin'
        output_path = str(data_dir / 'ml_training_data.csv')
    else:
        output_path = args.output
    
    print("╔═══════════════════════════════════════════════════╗")
    print("║  ninacatcoin ML Training Data Extractor v1.0      ║")
    print("╚═══════════════════════════════════════════════════╝")
    print()
    
    # Connect to daemon
    rpc = NinacatcoinRPC(host=args.host, port=args.port)
    
    print(f"→ Connecting to daemon at {args.host}:{args.port}...")
    info = rpc.get_info()
    
    chain_height = info.get('height', 0)
    target_height = info.get('target_height', 0)
    net_type = info.get('nettype', 'mainnet')
    
    print(f"  ✓ Connected! Network: {net_type}")
    print(f"  ✓ Chain height: {chain_height} blocks")
    if target_height > chain_height:
        print(f"  ⚠ Still syncing (target: {target_height})")
    
    if chain_height < 10:
        print("  ✗ Not enough blocks to extract meaningful data")
        sys.exit(1)
    
    # Fetch all block headers
    print(f"\n→ Fetching {chain_height} block headers...")
    start_time = time.time()
    
    headers = extract_all_blocks(rpc, chain_height, batch_size=args.batch_size)
    
    elapsed = time.time() - start_time
    print(f"  ⏱ Fetched in {elapsed:.1f}s ({len(headers)/elapsed:.0f} blocks/sec)")
    
    # Compute features and write CSV
    print(f"\n→ Computing ML features and writing CSV...")
    headers_to_csv(headers, output_path)
    
    print(f"\n✓ DONE! Training data ready at: {output_path}")
    print(f"  Next step: python train_models.py")
    print(f"  This will train Isolation Forest + GradientBoosting on your REAL blockchain data")


if __name__ == '__main__':
    main()
