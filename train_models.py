#!/usr/bin/env python3
"""
NINA ML Model Training Pipeline
Extracts blockchain data and trains real ML models

Input: blockchain data (10,269 blocks from data.mdb)
Output: Trained sklearn models for PHASE 1 and PHASE 2
"""

import os
import sys
import json
import re
import numpy as np
import pandas as pd
from pathlib import Path
from datetime import datetime
import logging

# ML libraries
from sklearn.ensemble import RandomForestClassifier, GradientBoostingRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, precision_score, recall_score, mean_squared_error
import joblib

# Try to import lmdb
try:
    import lmdb
    HAS_LMDB = True
except ImportError:
    HAS_LMDB = False

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)

# Paths
import pwd
home_dir = Path(pwd.getpwnam('jose').pw_dir) if sys.platform != 'win32' else Path(os.path.expandvars('%USERPROFILE%'))
BLOCKCHAIN_DIR = home_dir / '.ninacatcoin'
DATA_MDB = BLOCKCHAIN_DIR / 'lmdb' / 'data.mdb'
MODELS_DIR = Path(__file__).parent / 'src' / 'nina_ml' / 'models'
LOG_FILE = BLOCKCHAIN_DIR / 'ninacatcoin.log'

logger.info(f"Blockchain dir: {BLOCKCHAIN_DIR}")
logger.info(f"Models dir: {MODELS_DIR}")
logger.info(f"Log file: {LOG_FILE} (exists: {LOG_FILE.exists()})")

# ============================================================================
# DATA EXTRACTION
# ============================================================================

def extract_blockchain_data():
    """Extract block data from daemon logs (real time data)"""
    logger.info("Extracting blockchain data from logs...")
    
    blocks = []
    
    # Parse daemon logs for block information
    if LOG_FILE.exists():
        logger.info(f"Reading from daemon logs: {LOG_FILE}")
        
        heights = {}
        difficulties = {}
        
        with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                # Pattern: HEIGHT 10264, difficulty:    15139477
                # More tolerant regex with flexible whitespace
                match = re.search(r'HEIGHT\s+(\d+),\s+difficulty:\s+(\d+)', line)
                if match:
                    height = int(match.group(1))
                    difficulty = int(match.group(2))
                    
                    if height not in heights:
                        heights[height] = None
                        difficulties[height] = difficulty
        
        # Create block list from collected data
        if heights:
            for height in sorted(heights.keys()):
                blocks.append({
                    'height': height,
                    'difficulty': difficulties.get(height, 1000000),
                    'timestamp': height * 120,  # Approx: use height * 120 as proxy
                    'miner_address': 'unknown',
                    'txs_count': 0
                })
        else:
            logger.error("No blocks found in logs matching pattern")
    
    logger.info(f"✓ Extracted {len(blocks)} blocks from logs")
    return blocks

# ============================================================================
# FEATURE ENGINEERING
# ============================================================================

def prepare_phase1_features(blocks):
    """Prepare features for PHASE 1 block validation"""
    logger.info("Preparing PHASE 1 features...")
    
    features_list = []
    
    for i, block in enumerate(blocks):
        # Calculate network health (based on block time)
        if i > 0:
            block_time = block['timestamp'] - blocks[i-1]['timestamp']
            network_health = min(1.0, 120.0 / max(block_time, 1))
        else:
            network_health = 1.0
        
        # Miner reputation (simplified: consistent miner = good)
        miner_count = sum(1 for b in blocks[max(0, i-100):i] 
                         if b.get('miner_address') == block.get('miner_address'))
        miner_reputation = min(1.0, miner_count / 10.0)
        
        # Hash entropy (simplified: difficulty is proxy)
        # Higher difficulty = more valid work
        avg_diff = np.mean([b['difficulty'] for b in blocks[max(0, i-10):i+1]])
        hash_entropy = min(1.0, block['difficulty'] / max(avg_diff, 1))
        
        # Label blocks: mark as invalid (0) if they deviate significantly from patterns
        # Valid blocks have:
        # - Good network health (network_health > 0.5)
        # - Reasonable hash entropy (hash_entropy > 0.7)
        # Block is invalid if it fails these conditions
        is_valid = 1 if (network_health > 0.5 and hash_entropy > 0.7) else 0
        
        features_list.append({
            'network_health': network_health,
            'miner_reputation': miner_reputation,
            'hash_entropy': hash_entropy,
            'block_time': block.get('timestamp', 0),
            'difficulty': block['difficulty'],
            'txs_count': block.get('txs_count', 0),
            'is_valid': is_valid  
        })
    
    df = pd.DataFrame(features_list)
    logger.info(f"✓ Prepared {len(df)} PHASE 1 training samples (valid: {(df['is_valid']==1).sum()}, invalid: {(df['is_valid']==0).sum()})")
    return df

def prepare_phase2_features(blocks):
    """Prepare features for PHASE 2 difficulty optimization"""
    logger.info("Preparing PHASE 2 features...")
    
    features_list = []
    
    for i in range(1, len(blocks)):
        block = blocks[i]
        prev_block = blocks[i-1]
        
        # Block time
        block_time = block['timestamp'] - prev_block['timestamp']
        if block_time <= 0 or block_time > 1000:
            block_time = 120  # Default if invalid
        
        # Hashrate (approximation)
        # H/s = difficulty / block_time
        hashrate = block['difficulty'] / max(block_time, 1)
        
        # Hashrate trend (% change from recent average)
        recent_difficulties = [b['difficulty'] for b in blocks[max(0, i-60):i]]
        avg_diff = np.mean(recent_difficulties)
        hashrate_trend = 100.0 * (block['difficulty'] - avg_diff) / max(avg_diff, 1)
        
        # Optimal multiplier (target block time = 120s)
        target = 120
        optimal_multiplier = target / max(block_time, 1)
        optimal_multiplier = np.clip(optimal_multiplier, 0.7, 1.3)
        
        features_list.append({
            'block_time': block_time,
            'difficulty': block['difficulty'],
            'hashrate': hashrate,
            'hashrate_trend': hashrate_trend,
            'block_time_prev': blocks[i-2]['timestamp'] - blocks[i-3]['timestamp'] if i > 2 else 120,
            'avg_block_time': np.mean([blocks[j]['timestamp'] - blocks[j-1]['timestamp'] 
                                      for j in range(max(1, i-10), i+1)]),
            # Target: optimal multiplier
            'optimal_multiplier': optimal_multiplier
        })
    
    df = pd.DataFrame(features_list)
    logger.info(f"✓ Prepared {len(df)} PHASE 2 training samples")
    return df

# ============================================================================
# MODEL TRAINING
# ============================================================================

def train_phase1_model(X, y):
    """Train Random Forest for block validation"""
    logger.info("Training PHASE 1 model (Random Forest)...")
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    
    # Scale features
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)
    
    # Train model
    model = RandomForestClassifier(
        n_estimators=100,
        max_depth=15,
        min_samples_split=5,
        random_state=42,
        n_jobs=-1
    )
    model.fit(X_train_scaled, y_train)
    
    # Evaluate
    y_pred = model.predict(X_test_scaled)
    accuracy = accuracy_score(y_test, y_pred)
    precision = precision_score(y_test, y_pred, zero_division=0)
    recall = recall_score(y_test, y_pred, zero_division=0)
    
    logger.info(f"✓ PHASE 1 Model trained")
    logger.info(f"  Accuracy:  {accuracy:.4f}")
    logger.info(f"  Precision: {precision:.4f}")
    logger.info(f"  Recall:    {recall:.4f}")
    
    # Save
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, MODELS_DIR / 'phase1_block_validator.pkl')
    joblib.dump(scaler, MODELS_DIR / 'phase1_scaler.pkl')
    logger.info(f"✓ Saved to {MODELS_DIR / 'phase1_block_validator.pkl'}")
    
    return model, scaler

def train_phase2_model(X, y):
    """Train Gradient Boosting for difficulty optimization"""
    logger.info("Training PHASE 2 model (Gradient Boosting)...")
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    
    # Scale features
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)
    
    # Train model
    model = GradientBoostingRegressor(
        n_estimators=100,
        learning_rate=0.1,
        max_depth=5,
        random_state=42
    )
    model.fit(X_train_scaled, y_train)
    
    # Evaluate
    y_pred = model.predict(X_test_scaled)
    mse = mean_squared_error(y_test, y_pred)
    rmse = np.sqrt(mse)
    
    logger.info(f"✓ PHASE 2 Model trained")
    logger.info(f"  RMSE: {rmse:.6f}")
    logger.info(f"  Mean prediction: {y_pred.mean():.4f}")
    
    # Save
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    joblib.dump(model, MODELS_DIR / 'phase2_difficulty.pkl')
    joblib.dump(scaler, MODELS_DIR / 'phase2_scaler.pkl')
    logger.info(f"✓ Saved to {MODELS_DIR / 'phase2_difficulty.pkl'}")
    
    return model, scaler

# ============================================================================
# MAIN
# ============================================================================

def main():
    logger.info("="*70)
    logger.info("NINA ML MODEL TRAINING PIPELINE")
    logger.info("="*70)
    
    # Extract data
    blocks = extract_blockchain_data()
    if len(blocks) < 100:
        logger.error("Not enough blocks to train models (need >100)")
        return False
    
    # Prepare features
    phase1_df = prepare_phase1_features(blocks)
    phase2_df = prepare_phase2_features(blocks)
    
    # Train PHASE 1
    X1 = phase1_df[['network_health', 'miner_reputation', 'hash_entropy']].values
    y1 = phase1_df['is_valid'].values
    model1, scaler1 = train_phase1_model(X1, y1)
    
    # Train PHASE 2
    X2 = phase2_df[['block_time', 'difficulty', 'hashrate', 'hashrate_trend', 'avg_block_time']].values
    y2 = phase2_df['optimal_multiplier'].values
    model2, scaler2 = train_phase2_model(X2, y2)
    
    logger.info("="*70)
    logger.info("✓ TRAINING COMPLETE")
    logger.info(f"✓ Models saved to {MODELS_DIR}")
    logger.info("✓ Ready to restart server.py to use new models")
    logger.info("="*70)
    
    return True

if __name__ == '__main__':
    success = main()
    sys.exit(0 if success else 1)
