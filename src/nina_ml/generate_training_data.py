#!/usr/bin/env python3
"""
NINA ML Training Data Generator
Genera datos sintéticos de bloques para entrenar PHASE 1 Block Validator
"""

import json
import csv
import random
import time
import math
from datetime import datetime, timedelta
import numpy as np

def generate_realistic_blocks(num_blocks=5000, output_file="blocks.csv"):
    """
    Genera datos realistas de bloques blockchain para entrenamiento
    Características: timestamp, difficulty, txs_count, miner_address, etc.
    """
    print(f"[GENERATOR] Generando {num_blocks} bloques sintéticos para PHASE 1...")
    
    # Parámetros iniciales
    start_timestamp = int(time.time()) - (num_blocks * 120)  # ~2 min por bloque
    start_difficulty = 15000000
    block_data = []
    
    # Mineros comunes (simulados)
    miners = [f"miner_pool_{i}" for i in range(1, 11)]
    
    for block_height in range(num_blocks):
        timestamp = start_timestamp + (block_height * 120)
        
        # Difficulty ajustada (LWMA-1 simulation)
        difficulty = start_difficulty + random.randint(-1000000, 1000000)
        
        # Txs count (Poisson distribution)
        txs_count = max(1, int(np.random.poisson(50)))
        
        # Miner address (85% de probabilidad de minero conocido)
        if random.random() < 0.85:
            miner = random.choice(miners)
        else:
            miner = f"unknown_miner_{random.randint(1, 1000)}"
        
        # Network health (0-1 score)
        network_health = 0.95 + random.uniform(-0.1, 0.05)
        network_health = max(0.0, min(1.0, network_health))
        
        # Block age (time since previous)
        block_age = 120 + random.randint(-30, 30)
        
        # Hash entropy (0-256)
        hash_entropy = random.randint(200, 256)
        
        # Miner reputation (0-1 score, known miners have higher reputation)
        if "pool" in miner:
            miner_reputation = 0.9 + random.uniform(-0.1, 0.05)
        else:
            miner_reputation = 0.5 + random.uniform(-0.3, 0.3)
        miner_reputation = max(0.0, min(1.0, miner_reputation))
        
        # Label: is_valid (95% válidos, 5% anomalías)
        if random.random() < 0.95:
            is_valid = 1
        else:
            is_valid = 0  # Anomalía: timeout, suspicious pattern, etc
        
        block_data.append({
            'block_height': block_height,
            'timestamp': timestamp,
            'difficulty': difficulty,
            'txs_count': txs_count,
            'miner_address': miner,
            'network_health': round(network_health, 4),
            'block_age': block_age,
            'hash_entropy': hash_entropy,
            'miner_reputation': round(miner_reputation, 4),
            'is_valid': is_valid
        })
    
    # Guardar a CSV
    csv_path = output_file
    print(f"[GENERATOR] Guardando datos a {csv_path}...")
    
    with open(csv_path, 'w', newline='') as csvfile:
        fieldnames = list(block_data[0].keys())
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        
        writer.writeheader()
        for row in block_data:
            writer.writerow(row)
    
    print(f"✅ {num_blocks} bloques generados en {csv_path}")
    print(f"   • Bloques válidos: {sum(b['is_valid'] for b in block_data)}")
    print(f"   • Anomalías detectadas: {sum(1-b['is_valid'] for b in block_data)}")
    print(f"   • Rango difficulty: {min(b['difficulty'] for b in block_data)} - {max(b['difficulty'] for b in block_data)}")
    print(f"   • Txs promedio: {sum(b['txs_count'] for b in block_data) / num_blocks:.1f}")
    
    return csv_path

def validate_dataset(csv_path):
    """Valida que el dataset tenga formato correcto"""
    print(f"\n[VALIDATOR] Validando dataset {csv_path}...")
    
    try:
        with open(csv_path, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            
        print(f"✅ Dataset válido: {len(rows)} filas")
        print(f"   Columnas: {list(rows[0].keys())}")
        
        # Estadísticas básicas
        print(f"\n[STATS]")
        for col in ['difficulty', 'txs_count', 'network_health', 'miner_reputation']:
            values = [float(row[col]) for row in rows]
            print(f"   {col:20s}: min={min(values):.2f}, max={max(values):.2f}, mean={np.mean(values):.2f}")
        
        return True
    except Exception as e:
        print(f"❌ Error validando dataset: {e}")
        return False

if __name__ == "__main__":
    # Generar 5000 bloques de entrenamiento
    csv_file = "blocks.csv"
    generate_realistic_blocks(num_blocks=5000, output_file=csv_file)
    
    # Validar
    validate_dataset(csv_file)
    
    print(f"\n✅ Datos de entrenamiento listos en {csv_file}")
    print(f"   Próximo paso: python3 src/nina_ml/training/block_validator_train.py --data {csv_file}")
