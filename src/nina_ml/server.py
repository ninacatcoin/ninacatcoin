#!/usr/bin/env python3
"""
NINA ML Service v3.0
Socket-based microservice for AI/ML operations

Connects to NINA daemon via TCP socket (localhost:5556)
Handles 4 phases of evolution:
  - PHASE 1: Block validation anomaly detection (Random Forest)
  - PHASE 2: Difficulty optimization (Reinforcement Learning)
  - PHASE 3: Sybil detection reasoning (Graph Neural Network)
  - PHASE 4: Autonomous gas pricing (Policy Gradient)

Usage:
    python3 server.py --port 5556 --models-dir ./models
"""

import json
import socket
import threading
import logging
import argparse
import sys
import time
import subprocess
import csv
from collections import deque
from pathlib import Path
from typing import Dict, Optional, Any, List
from datetime import datetime, timezone
import traceback
import joblib
import numpy as np

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(name)s: %(message)s',
    handlers=[
        logging.FileHandler('nina_ml_service.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger('NINA-ML')


class NINAMLService:
    """
    Main ML service that listens for daemon requests and returns predictions
    """

    def __init__(self, host: str = '127.0.0.1', port: int = 5556, models_dir: str = './models'):
        self.host = host
        self.port = port
        self.models_dir = Path(models_dir)
        self.running = False
        self.client_threads = []
        self.use_trained_models = False
        
        # Model storage (will be loaded from disk)
        self.models = {
            'phase1_model': None,      # Random Forest classifier
            'phase1_scaler': None,     # StandardScaler for PHASE 1
            'phase2_model': None,      # Gradient Boosting regressor
            'phase2_scaler': None,     # StandardScaler for PHASE 2
            'phase1_features': None,   # Feature names list for PHASE 1
            'phase2_features': None,   # Feature names list for PHASE 2
            'block_validator': None,   # Legacy PHASE 1
            'difficulty_agent': None,  # Legacy PHASE 2
            'sybil_reasoner': None,    # PHASE 3: GNN reasoner
            'gas_optimizer': None      # PHASE 4: RL gas price agent
        }
        
        # Block history buffer for computing sliding window features at inference
        # Stores dicts: {solve_time, difficulty, network_health, miner_diversity, hash_entropy}
        self._block_history: deque = deque(maxlen=120)  # keep last 120 blocks
        self._history_lock = threading.Lock()
        
        # Auto-retrain state
        self._last_train_row_count = 0      # CSV row count at last training
        self._retrain_threshold = 1000      # Retrain every N new blocks
        self._retrain_interval = 600        # Check every 10 minutes (seconds)
        self._retrain_lock = threading.Lock()
        self._training_csv = None           # Set in start() after resolving home dir
        self._train_script = Path(__file__).parent.parent.parent / 'train_models.py'
        
        logger.info(f"NINA ML Service initialized: {host}:{port}, models_dir={models_dir}")

    def start(self):
        """Start the socket server"""
        self.running = True
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            server_socket.bind((self.host, self.port))
            server_socket.listen(5)
            logger.info(f"✓ ML Service listening on {self.host}:{self.port}")
            
            # Load models
            self._load_models()
            
            # Initialize auto-retrain: find training CSV and get baseline row count
            self._init_auto_retrain()
            
            # Start auto-retrain background thread
            retrain_thread = threading.Thread(
                target=self._auto_retrain_loop,
                daemon=True,
                name='auto-retrain'
            )
            retrain_thread.start()
            logger.info(f"✓ Auto-retrain thread started (check every {self._retrain_interval}s, "
                       f"retrain after {self._retrain_threshold} new blocks)")
            
            # Accept connections
            while self.running:
                try:
                    client_socket, client_address = server_socket.accept()
                    logger.info(f"→ New client connection from {client_address}")
                    
                    # Handle client in separate thread
                    client_thread = threading.Thread(
                        target=self._handle_client,
                        args=(client_socket, client_address),
                        daemon=True
                    )
                    client_thread.start()
                    self.client_threads.append(client_thread)
                    
                except KeyboardInterrupt:
                    logger.info("Shutdown signal received")
                    break
                except Exception as e:
                    if self.running:
                        logger.error(f"Error accepting connection: {e}")
                        
        except Exception as e:
            logger.error(f"Server error: {e}")
            traceback.print_exc()
        finally:
            server_socket.close()
            self.running = False
            logger.info("✗ ML Service stopped")

    def stop(self):
        """Stop the service gracefully"""
        self.running = False
        logger.info("Stopping ML Service...")

    def _init_auto_retrain(self):
        """Find training CSV, backfill from blockchain if needed, record baseline."""
        import os
        if sys.platform != 'win32':
            try:
                import pwd
                home = Path(pwd.getpwnam(os.getenv('USER', 'jose')).pw_dir)
            except Exception:
                home = Path.home()
        else:
            home = Path(os.path.expandvars('%USERPROFILE%'))
        
        self._training_csv = home / '.ninacatcoin' / 'ml_training_data.csv'
        self._rpc_url = 'http://127.0.0.1:19081/json_rpc'
        
        # Backfill CSV from local blockchain via RPC
        self._backfill_csv_from_blockchain()
        
        if self._training_csv.exists():
            self._last_train_row_count = self._count_csv_rows(self._training_csv)
            logger.info(f"[Auto-Retrain] Baseline CSV rows: {self._last_train_row_count}")
        else:
            logger.info(f"[Auto-Retrain] CSV not found yet: {self._training_csv}")
            self._last_train_row_count = 0

    # ------------------------------------------------------------------
    # CSV Backfill from Local Blockchain via RPC
    # ------------------------------------------------------------------
    def _rpc_call(self, method: str, params: dict = None) -> Optional[dict]:
        """Make a JSON-RPC call to the local daemon."""
        import urllib.request
        payload = json.dumps({
            "jsonrpc": "2.0",
            "id": "0",
            "method": method,
            "params": params or {}
        }).encode('utf-8')
        try:
            req = urllib.request.Request(
                self._rpc_url,
                data=payload,
                headers={'Content-Type': 'application/json'}
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = json.loads(resp.read().decode('utf-8'))
                if 'error' in data:
                    logger.warning(f"[RPC] {method} error: {data['error']}")
                    return None
                return data.get('result')
        except Exception as e:
            logger.debug(f"[RPC] {method} failed: {e}")
            return None

    def _get_last_csv_height(self) -> int:
        """Read the last height already present in the training CSV. Returns -1 if empty."""
        if not self._training_csv.exists():
            return -1
        try:
            last_height = -1
            with open(self._training_csv, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('height'):
                        continue
                    parts = line.split(',')
                    if parts:
                        try:
                            last_height = int(parts[0])
                        except ValueError:
                            pass
            return last_height
        except Exception:
            return -1

    def _backfill_csv_from_blockchain(self):
        """
        Auto-generate training CSV rows from the local blockchain via RPC.
        
        Each node has the SAME blockchain, so sharing the CSV is unnecessary.
        Instead, each node fills its own CSV directly from its local chain.
        
        This runs at startup to catch up any blocks the daemon processed
        before the ML server was running, or to regenerate a missing/partial CSV.
        """
        logger.info("[Backfill] Checking if CSV needs backfill from local blockchain...")
        
        # Check daemon is reachable
        info = self._rpc_call('get_info')
        if not info:
            logger.info("[Backfill] Daemon not reachable — skipping backfill (will retry next restart)")
            return
        
        chain_height = info.get('height', 0) - 1  # 'height' is next block, top is height-1
        if chain_height <= 0:
            logger.info("[Backfill] Chain is empty, nothing to backfill")
            return
        
        last_csv_height = self._get_last_csv_height()
        start_height = last_csv_height + 1
        
        if start_height > chain_height:
            logger.info(f"[Backfill] CSV already up-to-date (last={last_csv_height}, chain={chain_height})")
            return
        
        blocks_to_fill = chain_height - start_height + 1
        logger.info(f"[Backfill] Filling {blocks_to_fill} blocks ({start_height} → {chain_height}) from local blockchain...")
        
        # Ensure CSV exists with header
        write_header = not self._training_csv.exists() or self._count_csv_rows(self._training_csv) == 0
        
        filled = 0
        prev_timestamp = None
        
        # If we need solve_time for block N, we need block N-1 timestamp
        if start_height > 0:
            prev_result = self._rpc_call('get_block_header_by_height', {'height': start_height - 1})
            if prev_result and 'block_header' in prev_result:
                prev_timestamp = prev_result['block_header'].get('timestamp')
        
        with open(str(self._training_csv), 'a', newline='') as f:
            writer = csv.writer(f)
            if write_header:
                writer.writerow([
                    'height', 'timestamp', 'solve_time', 'difficulty', 'cumulative_difficulty',
                    'txs_count', 'block_size_bytes', 'network_health', 'miner_diversity',
                    'hash_entropy', 'ml_confidence', 'ml_risk_score', 'block_accepted'
                ])
            
            # Batch fetch using get_block_headers_range (more efficient)
            BATCH_SIZE = 100
            for batch_start in range(start_height, chain_height + 1, BATCH_SIZE):
                batch_end = min(batch_start + BATCH_SIZE - 1, chain_height)
                
                result = self._rpc_call('get_block_headers_range', {
                    'start_height': batch_start,
                    'end_height': batch_end
                })
                
                if not result or 'headers' not in result:
                    # Fallback: fetch one by one
                    for h in range(batch_start, batch_end + 1):
                        r = self._rpc_call('get_block_header_by_height', {'height': h})
                        if r and 'block_header' in r:
                            hdr = r['block_header']
                            self._write_block_row(writer, hdr, prev_timestamp)
                            prev_timestamp = hdr.get('timestamp')
                            filled += 1
                    continue
                
                for hdr in result['headers']:
                    self._write_block_row(writer, hdr, prev_timestamp)
                    prev_timestamp = hdr.get('timestamp')
                    filled += 1
                
                if filled % 1000 == 0 and filled > 0:
                    f.flush()
                    logger.info(f"[Backfill] Progress: {filled}/{blocks_to_fill} blocks...")
        
        logger.info(f"[Backfill] ✓ Filled {filled} blocks into CSV ({self._training_csv})")

    def _write_block_row(self, writer, hdr: dict, prev_timestamp: Optional[int]):
        """Write a single block row to CSV from RPC block_header data."""
        ts = hdr.get('timestamp', 0)
        solve_time = 120  # default
        if prev_timestamp and ts > prev_timestamp:
            solve_time = ts - prev_timestamp
        
        # Compute simple hash entropy from block hash
        block_hash = hdr.get('hash', '')
        hash_entropy = 0
        if block_hash:
            for i in range(1, len(block_hash)):
                if block_hash[i] != block_hash[i - 1]:
                    hash_entropy += 1
        
        writer.writerow([
            hdr.get('height', 0),
            ts,
            solve_time,
            hdr.get('difficulty', 0),
            hdr.get('cumulative_difficulty', 0),
            hdr.get('num_txes', 0) + 1,  # +1 coinbase, same as daemon
            hdr.get('block_weight', hdr.get('block_size', 0)),
            0.950000,       # network_health (same default as daemon)
            0.500000,       # miner_diversity (same default as daemon)
            hash_entropy,
            0.000000,       # ml_confidence (not available)
            0.000000,       # ml_risk_score (not available)
            1               # block_accepted (it's in the chain)
        ])
    
    def _count_csv_rows(self, csv_path: Path) -> int:
        """Count data rows in CSV (excluding header)."""
        try:
            with open(csv_path, 'r') as f:
                return sum(1 for _ in f) - 1  # minus header
        except Exception:
            return 0
    
    def _auto_retrain_loop(self):
        """
        Background thread: periodically check if enough new blocks have accumulated
        in the training CSV, and if so, run the full training pipeline and hot-reload.
        """
        logger.info("[Auto-Retrain] Background monitor started")
        
        while self.running:
            try:
                time.sleep(self._retrain_interval)
                
                if not self.running:
                    break
                
                if self._training_csv is None or not self._training_csv.exists():
                    continue
                
                current_rows = self._count_csv_rows(self._training_csv)
                new_rows = current_rows - self._last_train_row_count
                
                if new_rows < self._retrain_threshold:
                    logger.debug(f"[Auto-Retrain] {new_rows}/{self._retrain_threshold} new blocks, waiting...")
                    continue
                
                logger.info(f"[Auto-Retrain] {new_rows} new blocks detected (threshold={self._retrain_threshold}). "
                           f"Starting retraining...")
                
                success = self._run_training()
                
                if success:
                    self._last_train_row_count = current_rows
                    logger.info("[Auto-Retrain] Retraining succeeded. Hot-reloading models...")
                    self.reload_models()
                    logger.info("[Auto-Retrain] ✓ Models reloaded successfully after retraining")
                else:
                    logger.error("[Auto-Retrain] ✗ Retraining failed — keeping old models")
                    
            except Exception as e:
                logger.error(f"[Auto-Retrain] Error in retrain loop: {e}")
                traceback.print_exc()
    
    def _run_training(self) -> bool:
        """
        Execute the training pipeline.
        First tries to import train_models.py directly (faster, in-process).
        Falls back to subprocess if import fails.
        """
        with self._retrain_lock:
            try:
                # Try in-process training by importing the module
                train_script = self._train_script
                if train_script.exists():
                    logger.info(f"[Auto-Retrain] Running training script: {train_script}")
                    result = subprocess.run(
                        [sys.executable, str(train_script)],
                        capture_output=True,
                        text=True,
                        timeout=300,  # 5 min max
                        cwd=str(train_script.parent)
                    )
                    
                    if result.returncode == 0:
                        logger.info("[Auto-Retrain] ✓ Training completed successfully")
                        # Log last few lines of output
                        for line in result.stdout.strip().split('\n')[-5:]:
                            logger.info(f"  [train] {line}")
                        return True
                    else:
                        logger.error(f"[Auto-Retrain] Training failed (exit code {result.returncode})")
                        for line in result.stderr.strip().split('\n')[-5:]:
                            logger.error(f"  [train] {line}")
                        return False
                else:
                    logger.error(f"[Auto-Retrain] Training script not found: {train_script}")
                    return False
                    
            except subprocess.TimeoutExpired:
                logger.error("[Auto-Retrain] Training timed out after 5 minutes")
                return False
            except Exception as e:
                logger.error(f"[Auto-Retrain] Training error: {e}")
                traceback.print_exc()
                return False

    def _load_models(self):
        """
        Load pre-trained models from disk.
        v2.0: Supports Isolation Forest (PHASE 1) + GradientBoosting (PHASE 2)
        Also loads feature lists so we know which features each model expects.
        """
        logger.info("Loading ML models...")
        
        try:
            # PHASE 1: Isolation Forest (anomaly detection)
            phase1_model_path = self.models_dir / 'phase1_anomaly_detector.pkl'
            phase1_scaler_path = self.models_dir / 'phase1_scaler.pkl'
            phase1_features_path = self.models_dir / 'phase1_features.pkl'
            
            # PHASE 2: GradientBoosting (difficulty prediction)
            phase2_model_path = self.models_dir / 'phase2_difficulty.pkl'
            phase2_scaler_path = self.models_dir / 'phase2_scaler.pkl'
            phase2_features_path = self.models_dir / 'phase2_features.pkl'
            
            # Load PHASE 1
            if phase1_model_path.exists() and phase1_scaler_path.exists():
                self.models['phase1_model'] = joblib.load(phase1_model_path)
                self.models['phase1_scaler'] = joblib.load(phase1_scaler_path)
                if phase1_features_path.exists():
                    self.models['phase1_features'] = joblib.load(phase1_features_path)
                logger.info("✓ PHASE 1 loaded (Isolation Forest — anomaly detection)")
            else:
                # Legacy: try old Random Forest model
                legacy_path = self.models_dir / 'phase1_block_validator.pkl'
                if legacy_path.exists() and phase1_scaler_path.exists():
                    self.models['phase1_model'] = joblib.load(legacy_path)
                    self.models['phase1_scaler'] = joblib.load(phase1_scaler_path)
                    logger.info("✓ PHASE 1 loaded (legacy Random Forest)")
                else:
                    logger.warning("⚠ PHASE 1 model not found")
            
            # Load PHASE 2
            if phase2_model_path.exists() and phase2_scaler_path.exists():
                self.models['phase2_model'] = joblib.load(phase2_model_path)
                self.models['phase2_scaler'] = joblib.load(phase2_scaler_path)
                if phase2_features_path.exists():
                    self.models['phase2_features'] = joblib.load(phase2_features_path)
                logger.info("✓ PHASE 2 loaded (GradientBoosting — difficulty prediction)")
            else:
                logger.warning("⚠ PHASE 2 model not found")
            
            self.use_trained_models = (self.models['phase1_model'] is not None 
                                       or self.models['phase2_model'] is not None)
            
            if self.use_trained_models:
                logger.info("✓ ML models active — using real predictions")
            else:
                logger.warning("⚠ No trained models found — falling back to heuristics")
                
        except Exception as e:
            logger.error(f"Error loading models: {e}")
            import traceback
            traceback.print_exc()
            self.use_trained_models = False

    # =========================================================================
    # BLOCK HISTORY — sliding window feature computation for inference
    # =========================================================================

    def _record_block(self, features: Dict):
        """
        Record a block's features into the history buffer.
        Called on every PHASE 1 / PHASE 2 request so that subsequent
        requests can compute sliding window stats.
        """
        entry = {
            'solve_time':      float(features.get('solve_time', features.get('block_time', 120))),
            'difficulty':      float(features.get('difficulty', features.get('current_diff', 1))),
            'network_health':  float(features.get('network_health', 0.5)),
            'miner_diversity': float(features.get('miner_diversity', features.get('miner_reputation', 0.5))),
            'hash_entropy':    float(features.get('hash_entropy', 128)),
        }
        with self._history_lock:
            self._block_history.append(entry)

    def _compute_window_features(self, current: Dict) -> Dict:
        """
        Given the current block's raw features *and* the accumulated history,
        compute the same sliding-window features used during training.

        Returns a dict mapping feature_name -> value, matching the columns
        produced by engineer_features() in train_models.py.
        """
        with self._history_lock:
            history = list(self._block_history)

        solve_time  = float(current.get('solve_time', current.get('block_time', 120)))
        difficulty  = float(current.get('difficulty',  current.get('current_diff', 1)))
        net_health  = float(current.get('network_health', 0.5))
        miner_div   = float(current.get('miner_diversity', current.get('miner_reputation', 0.5)))
        h_entropy   = float(current.get('hash_entropy', 128))

        # Build arrays including the current block at the end
        solve_arr   = np.array([b['solve_time']     for b in history] + [solve_time])
        diff_arr    = np.array([b['difficulty']      for b in history] + [difficulty])
        health_arr  = np.array([b['network_health']  for b in history] + [net_health])

        def _tail_mean(arr, n):
            tail = arr[-n:] if len(arr) >= n else arr
            return float(np.mean(tail)) if len(tail) > 0 else 0.0

        def _tail_std(arr, n):
            tail = arr[-n:] if len(arr) >= n else arr
            return float(np.std(tail, ddof=1)) if len(tail) > 1 else 0.0

        def _tail_median(arr, n):
            tail = arr[-n:] if len(arr) >= n else arr
            return float(np.median(tail)) if len(tail) > 0 else 0.0

        w10_solve_mean  = _tail_mean(solve_arr, 10)
        w10_solve_std   = _tail_std(solve_arr, 10)
        w30_solve_mean  = _tail_mean(solve_arr, 30)
        w30_solve_std   = _tail_std(solve_arr, 30)
        w30_diff_mean   = _tail_mean(diff_arr, 30)
        w60_solve_mean  = _tail_mean(solve_arr, 60)
        w60_diff_mean   = _tail_mean(diff_arr, 60)
        w30_health_mean = _tail_mean(health_arr, 30)

        # Derived features
        prev_diff = float(diff_arr[-2]) if len(diff_arr) >= 2 else difficulty
        diff_change_pct = ((difficulty - prev_diff) / prev_diff * 100.0) if prev_diff != 0 else 0.0
        solve_deviation = solve_time - 120.0
        solve_deviation_abs = abs(solve_deviation)
        diff_vs_avg_ratio = (difficulty / w30_diff_mean) if w30_diff_mean != 0 else 1.0
        solve_cv_30 = (w30_solve_std / w30_solve_mean) if w30_solve_mean != 0 else 0.0

        return {
            # Raw features
            'solve_time':        solve_time,
            'difficulty':        difficulty,
            'network_health':    net_health,
            'miner_diversity':   miner_div,
            'hash_entropy':      h_entropy,
            # Window-10
            'w10_solve_mean':    w10_solve_mean,
            'w10_solve_std':     w10_solve_std,
            # Window-30
            'w30_solve_mean':    w30_solve_mean,
            'w30_solve_std':     w30_solve_std,
            'w30_diff_mean':     w30_diff_mean,
            'w30_health_mean':   w30_health_mean,
            # Window-60
            'w60_solve_mean':    w60_solve_mean,
            'w60_diff_mean':     w60_diff_mean,
            # Derived
            'diff_change_pct':   diff_change_pct,
            'solve_deviation':   solve_deviation,
            'solve_deviation_abs': solve_deviation_abs,
            'diff_vs_avg_ratio': diff_vs_avg_ratio,
            'solve_cv_30':       solve_cv_30,
        }

    def _handle_client(self, client_socket: socket.socket, client_address: tuple):
        """
        Handle individual client connection
        Receive requests, process, send responses
        Keeps connection alive for multiple requests (persistent connection)
        """
        try:
            client_socket.settimeout(600.0)  # 30 second idle timeout instead of 5s
            while self.running:
                # Read line-by-line with explicit handling
                
                try:
                    # Read until newline
                    buffer = b""
                    while b"\n" not in buffer:
                        chunk = client_socket.recv(1)
                        if not chunk:
                            break
                        buffer += chunk
                    
                    if not buffer:
                        break
                    
                    # Decode and parse JSON
                    line = buffer.decode('utf-8').strip()
                    if not line:
                        continue
                    
                    message = json.loads(line)
                    logger.info(f"[{client_address[0]}] Received: {message.get('phase', message.get('type', '?'))}")
                    
                    # Route to appropriate handler
                    response = self._process_message(message)
                    
                    # Send response with newline
                    response_str = json.dumps(response) + '\n'
                    client_socket.sendall(response_str.encode('utf-8'))
                    
                except socket.timeout:
                    logger.warning(f"Socket timeout for {client_address}")
                    break
                except json.JSONDecodeError as e:
                    logger.error(f"Invalid JSON from {client_address}: {e}")
                    error_response = {
                        'type': 'error',
                        'error': f'Invalid JSON: {str(e)}'
                    }
                    try:
                        client_socket.sendall((json.dumps(error_response) + '\n').encode('utf-8'))
                    except:
                        pass
                    break
                    
        except Exception as e:
            logger.error(f"Error handling client {client_address}: {e}")
            traceback.print_exc()
        finally:
            try:
                client_socket.close()
            except:
                pass
            logger.info(f"← Client disconnected: {client_address}")

    def _process_message(self, message: Dict[str, Any]) -> Dict[str, Any]:
        """
        Route incoming message to appropriate handler
        Supports both new format (type/tier) and daemon format (phase)
        """
        msg_type = message.get('type')
        tier = message.get('tier')
        phase = message.get('phase')
        decision_id = message.get('decision_id', 'unknown')
        
        # Support new daemon format with "phase" field
        if phase == 'PHASE_1_BLOCK_VALIDATE':
            return self._phase1_block_validation(decision_id, message)
        elif phase == 'PHASE_2_DIFFICULTY_OPTIMIZE':
            return self._phase2_difficulty_optimization(decision_id, message)
        elif phase == 'PHASE_3_SYBIL_DETECT':
            return self._phase3_sybil_detection(decision_id, message)
        elif phase == 'PHASE_4_GAS_PRICE':
            return self._phase4_gas_pricing(decision_id, message)
        # Support old format with "type" field
        elif msg_type == 'feature_request':
            return self._handle_feature_request(
                tier, decision_id, message.get('features', {})
            )
        elif msg_type == 'training_data':
            return self._handle_training_data(message)
        elif msg_type == 'health_check':
            return self._handle_health_check()
        elif msg_type == 'reload_models':
            success = self.reload_models()
            return {
                'type': 'acknowledgment',
                'status': 'reloaded' if success else 'reload_failed',
                'use_trained_models': self.use_trained_models
            }
        elif msg_type == 'shutdown':
            self.stop()
            return {'type': 'acknowledgment', 'status': 'shutting_down'}
        else:
            return {
                'type': 'error',
                'error': f'Unknown message type: {msg_type}, phase: {phase}'
            }

    def _handle_feature_request(self, tier: int, decision_id: str, features: Dict) -> Dict:
        """
        Main entry point for prediction requests
        Routes to correct model based on TIER
        """
        try:
            if tier == 4:  # Block validation or gas pricing
                # Determine if it's block validation or gas pricing based on features
                if 'block_hash' in features:
                    return self._phase1_block_validation(decision_id, features)
                elif 'mempool_size' in features:
                    return self._phase4_gas_pricing(decision_id, features)
            elif tier == 5:  # Difficulty adjustment
                return self._phase2_difficulty_optimization(decision_id, features)
            elif tier == 2 or tier == 6:  # P2P network analysis
                return self._phase3_sybil_detection(decision_id, features)
            else:
                return {
                    'type': 'error',
                    'decision_id': decision_id,
                    'error': f'Unknown TIER: {tier}'
                }
        except Exception as e:
            logger.error(f"Error processing feature request {decision_id}: {e}")
            traceback.print_exc()
            return {
                'type': 'error',
                'decision_id': decision_id,
                'error': str(e)
            }

    def _phase1_block_validation(self, decision_id: str, features: Dict) -> Dict:
        """
        PHASE 1: Block validation using Isolation Forest (anomaly detection)
        
        v2.0: Uses unsupervised anomaly detection — no labels needed.
        The model learns what "normal" blocks look like and flags outliers.
        
        Input features from daemon (blockchain.cpp):
            - network_health: % of healthy blocks in recent window  
            - miner_reputation: difficulty variance as miner diversity proxy
            - hash_entropy: actual bit transitions in block hash
            - solve_time, difficulty, txs_count, block_size (if available)
            
        Output: confidence, is_valid, risk_score, action
        """
        logger.info(f"[PHASE 1] Block validation request for {decision_id}")
        
        try:
            # Extract features from daemon request
            network_health = float(features.get('network_health', 0.95))
            miner_reputation = float(features.get('miner_reputation', 0.8))
            hash_entropy = int(features.get('hash_entropy', 200))
            solve_time = float(features.get('solve_time', 120))
            difficulty = float(features.get('difficulty', 100000))
            txs_count = int(features.get('txs_count', 1))
            block_size = int(features.get('block_size', 1000))
            
            # Record this block into the history buffer for window computations
            self._record_block(features)
            
            # Compute sliding window features (w10, w30, w60, derived)
            window_feats = self._compute_window_features(features)
            
            use_model = False
            model_used = "(Heuristic)"
            
            # TRY TRAINED MODEL first
            if self.use_trained_models and self.models['phase1_model'] is not None:
                try:
                    model = self.models['phase1_model']
                    scaler = self.models['phase1_scaler']
                    
                    # Build feature vector — use saved feature list if available
                    feature_names = self.models.get('phase1_features', None)
                    
                    if feature_names is not None:
                        # window_feats already contains all sliding-window features
                        # plus raw features that match train_models.py columns
                        X = np.array([[window_feats.get(f, 0.0) for f in feature_names]])
                    else:
                        # Legacy 3-feature model
                        X = np.array([[network_health, miner_reputation, hash_entropy]])
                    
                    # Scale
                    X_scaled = scaler.transform(X)
                    
                    # Detect if this is an Isolation Forest or Random Forest
                    model_type = type(model).__name__
                    
                    if model_type == 'IsolationForest':
                        # IsolationForest: decision_function returns anomaly score
                        # Negative = anomaly, positive = normal
                        anomaly_score = float(model.decision_function(X_scaled)[0])
                        prediction = int(model.predict(X_scaled)[0])  # 1=normal, -1=anomaly
                        
                        # Convert anomaly score to confidence (0..1)
                        # decision_function range is roughly [-0.5, 0.5]
                        # Map to [0, 1] where 1 = very normal, 0 = very anomalous
                        confidence = float(np.clip((anomaly_score + 0.5), 0.0, 1.0))
                        
                        is_valid = (prediction == 1)
                        risk_score = 1.0 - confidence
                        action = "ACCEPT" if is_valid else "REVIEW"
                        model_used = "(Isolation Forest)"
                        
                    else:
                        # Legacy Random Forest: use predict_proba
                        predictions_proba = model.predict_proba(X_scaled)
                        if predictions_proba.shape[1] >= 2:
                            confidence = float(predictions_proba[0][1])
                        else:
                            confidence = float(predictions_proba[0][0])
                        
                        is_valid = confidence >= 0.5
                        risk_score = 1.0 - confidence
                        action = "ACCEPT" if is_valid else "REVIEW"
                        model_used = "(Random Forest)"
                    
                    use_model = True
                    
                except Exception as e:
                    logger.warning(f"Model prediction failed, falling back to heuristics: {e}")
                    use_model = False
            
            # FALLBACK: Heuristic (real but simple)
            if not use_model:
                # Score each dimension independently
                # network_health: 0.0 (bad) to 1.0 (good)
                health_score = max(0.0, min(1.0, network_health))
                
                # miner_reputation/diversity: penalize extremes
                # Very low diversity (< 0.3) is suspicious
                diversity_score = max(0.0, min(1.0, miner_reputation))
                
                # hash_entropy: should be ~128 for 256-bit hash
                # Penalize very low entropy (< 100) — suspicious hash
                entropy_norm = max(0.0, min(1.0, hash_entropy / 200.0))
                
                # solve_time: should be near target (120s)
                # Penalize extreme deviations
                time_ratio = solve_time / 120.0
                time_score = max(0.0, 1.0 - abs(1.0 - time_ratio) * 0.5)
                
                # Weighted combination
                confidence = (health_score * 0.3 + diversity_score * 0.2 + 
                            entropy_norm * 0.2 + time_score * 0.3)
                
                is_valid = confidence >= 0.5
                risk_score = 1.0 - confidence
                action = "ACCEPT" if is_valid else "REVIEW"
            
            result = {
                'phase': 'PHASE_1_BLOCK_VALIDATE',
                'decision_id': decision_id,
                'confidence': round(confidence, 4),
                'is_valid': is_valid,
                'risk_score': round(risk_score, 4),
                'action': action,
                'model': model_used,
                'components': {
                    'network_health': round(network_health, 4),
                    'miner_reputation': round(miner_reputation, 4),
                    'hash_entropy': hash_entropy
                },
                'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z')
            }
            
            logger.info(f"✓ PHASE 1 {decision_id}: confidence={result['confidence']}, action={action}, model={model_used}")
            return result
        
        except Exception as e:
            logger.error(f"✗ PHASE 1 error for {decision_id}: {e}")
            return {
                'phase': 'PHASE_1_BLOCK_VALIDATE',
                'decision_id': decision_id,
                'error': str(e),
                'confidence': 0.0,
                'action': 'REJECT',
                'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z')
            }

    def _phase2_difficulty_optimization(self, decision_id: str, features: Dict) -> Dict:
        """
        PHASE 2: Difficulty optimization - COMPLEMENTS LWMA algorithm
        
        Purpose: Monitor hashrate trends and help synchronize Network Difficulty 
        with actual HashRate (H/s). Works alongside LWMA to prevent difficulty oscillation.
        
        Input: 
            - current_difficulty: Current network difficulty
            - block_time: Time to mine last block (seconds)
            - hashrate: Current network hashrate (H/s)
            - hashrate_trend: Hashrate trend (% change from avg)
            - target_block_time: Target time per block (default 120s)
            
        Output:
            - recommended_difficulty: Suggested difficulty adjustment
            - hashrate_prediction: Predicted hashrate direction
            - confidence: Confidence in recommendation
            - action: INCREASE/DECREASE/MAINTAIN
            - lwma_complement: Info to help LWMA algorithm
        """
        logger.info(f"[PHASE 2] Difficulty optimization request for {decision_id}")
        
        try:
            # Extract features from daemon
            current_diff = float(features.get('difficulty', 100000))
            block_time = float(features.get('block_time', 120))
            hashrate = float(features.get('hashrate', 0))
            hashrate_trend = float(features.get('hashrate_trend', 0.0))
            target_block_time = float(features.get('target_block_time', 120))
            avg_block_time = float(features.get('avg_block_time', 120))
            
            # Record this block and compute sliding window features
            # Map daemon keys so _record_block sees solve_time & difficulty
            record = dict(features)
            record.setdefault('solve_time', block_time)
            record.setdefault('difficulty', current_diff)
            self._record_block(record)
            window_feats = self._compute_window_features(record)
            
            model_used = "(Heuristic)"
            
            # TRY TRAINED MODEL first
            if self.use_trained_models and self.models['phase2_model'] is not None:
                try:
                    model = self.models['phase2_model']
                    scaler = self.models['phase2_scaler']
                    feature_names = self.models.get('phase2_features', None)
                    
                    if feature_names is not None:
                        # window_feats has all sliding-window features matching train_models.py
                        X = np.array([[window_feats.get(f, 0.0) for f in feature_names]])
                    else:
                        # Legacy 5-feature model
                        X = np.array([[
                            block_time,
                            current_diff,
                            hashrate,
                            hashrate_trend,
                            avg_block_time
                        ]])
                    
                    # Scale using trained scaler
                    X_scaled = scaler.transform(X)
                    
                    # Get prediction from Gradient Boosting
                    multiplier_prediction = float(model.predict(X_scaled)[0])
                    
                    # Ensure multiplier is reasonable (0.8 to 1.2)
                    final_multiplier = float(np.clip(multiplier_prediction, 0.8, 1.2))
                    
                    # Determine action based on multiplier
                    if final_multiplier > 1.01:
                        action = "INCREASE"
                    elif final_multiplier < 0.99:
                        action = "DECREASE"
                    else:
                        action = "MAINTAIN"
                    
                    recommended_diff = int(current_diff * final_multiplier)
                    confidence = 0.90  # High confidence in trained model
                    hashrate_prediction = "STABLE"
                    
                    model_used = "(Gradient Boosting)"
                    use_model = True
                    
                except Exception as e:
                    logger.warning(f"Model prediction failed, falling back to heuristics: {e}")
                    use_model = False
            else:
                use_model = False
            
            # FALLBACK: Heuristic implementation
            if not use_model:
                # LWMA already adjusts based on block time
                # PHASE 2 analyzes hashrate trends to COMPLEMENT LWMA decisions
                
                # Step 1: Primary signal from block timing
                # Blocks too slow → decrease difficulty (hard to mine)
                # Blocks too fast → increase difficulty (too easy to mine)
                block_time_deviation = block_time - target_block_time
                
                if block_time_deviation >= 10:  # Blocks slower than target (120+10=130s or more)
                    primary_action = "DECREASE"
                    time_multiplier = 0.97  # Decrease to make mining easier
                elif block_time_deviation <= -10:  # Blocks faster than target (120-10=110s or less)
                    primary_action = "INCREASE"
                    time_multiplier = 1.03  # Increase to make mining harder
                else:
                    primary_action = "MAINTAIN"
                    time_multiplier = 1.0
                
                # Step 2: Refine with hashrate trend (predict future difficulty needs)
                if hashrate_trend > 2.0:  # Significant growth
                    hashrate_prediction = "GROWING"
                    # Growing hashrate → blocks will get faster → increase difficulty more
                    if primary_action == "INCREASE":
                        trend_multiplier = 1.03  # Strong increase
                    elif primary_action == "MAINTAIN":
                        trend_multiplier = 1.01  # Gentle increase
                    else:  # DECREASE
                        trend_multiplier = 0.99  # Less aggressive decrease
                elif hashrate_trend < -2.0:  # Significant decline
                    hashrate_prediction = "DECLINING"
                    # Declining hashrate → blocks will get slower → decrease difficulty more
                    if primary_action == "DECREASE":
                        trend_multiplier = 0.97  # Strong decrease
                    elif primary_action == "MAINTAIN":
                        trend_multiplier = 0.99  # Gentle decrease
                    else:  # INCREASE
                        trend_multiplier = 1.01  # Less aggressive increase
                else:
                    hashrate_prediction = "STABLE"
                    trend_multiplier = 1.0
                
                # Step 3: Combine signals
                final_multiplier = time_multiplier * trend_multiplier
                recommended_diff = int(current_diff * final_multiplier)
                
                # Step 4: Determine final action based on combined multiplier
                if final_multiplier > 1.01:
                    action = "INCREASE"
                elif final_multiplier < 0.99:
                    action = "DECREASE"
                else:
                    action = "MAINTAIN"
                
                # Step 5: Calculate confidence
                # Higher confidence when hashrate is stable OR when signals are aligned
                # Lower confidence when hashrate is volatile
                if hashrate_trend == 0:
                    volatility_factor = 0.0  # No volatility
                else:
                    volatility_factor = min(1.0, abs(hashrate_trend) / 10.0)  # Normalized volatility
                
                confidence = 0.95 - (volatility_factor * 0.25)  # 0.70-0.95 range
                confidence = min(0.98, max(0.70, confidence))
            
            result = {
                'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
                'decision_id': decision_id,
                'recommended_difficulty': recommended_diff,
                'difficulty_multiplier': round(final_multiplier, 4),
                'action': action,
                'confidence': round(confidence, 4),
                'model': model_used,
                'hashrate_analysis': {
                    'current_trend': hashrate_trend,
                    'prediction': hashrate_prediction,
                    'current_hashrate': hashrate,
                    'block_time': block_time,
                    'target_block_time': target_block_time,
                    'block_time_deviation': round(block_time - target_block_time, 2)
                },
                'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z')
            }
            
            logger.info(f"✓ PHASE 2 {decision_id}: multiplier={final_multiplier:.4f}, action={action}, "
                       f"confidence={confidence:.4f}, model={model_used}")
            return result
        
        except Exception as e:
            logger.error(f"✗ PHASE 2 error for {decision_id}: {e}")
            return {
                'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
                'decision_id': decision_id,
                'error': str(e),
                'action': 'MAINTAIN',
                'confidence': 0.0,
                'timestamp': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z')
            }

    def _phase3_sybil_detection(self, decision_id: str, features: Dict) -> Dict:
        """
        PHASE 3: Sybil attack detection using reasoning
        Input: peer_ip, connection_pattern, behavior_anomaly_score, ip_cluster_size
        Output: sybil_probability, reasoning, recommended_action
        """
        logger.info(f"[PHASE 3] Sybil detection request for {decision_id}")
        
        # TODO: Use actual GNN reasoner
        # For now, simple heuristic logic
        
        peer_ip = features.get('peer_ip', '0.0.0.0')
        cluster_size = features.get('ip_cluster_size', 1)
        behavior_anomaly = features.get('behavior_anomaly_score', 0.0)
        
        # Multiple IPs from same network + anomalous behavior = Sybil
        sybil_probability = min(0.99, (cluster_size / 20.0) * 0.5 + behavior_anomaly * 0.5)
        
        return {
            'type': 'prediction',
            'decision_id': decision_id,
            'phase': 3,
            'prediction': {
                'sybil_probability': float(sybil_probability),
                'reasoning': f'IP {peer_ip} in cluster of {cluster_size}, behavior anomaly {behavior_anomaly:.2f}',
                'recommended_action': 'BLOCK' if sybil_probability > 0.7 else 'MONITOR'
            },
            'timestamp': datetime.now().isoformat()
        }

    def _phase4_gas_pricing(self, decision_id: str, features: Dict) -> Dict:
        """
        PHASE 4: Autonomous gas pricing optimization
        Input: mempool_size, avg_tx_age, network_congestion, current_gas_price
        Output: recommended_gas_price, expected_confirmation_blocks, action
        """
        logger.info(f"[PHASE 4] Gas pricing optimization request for {decision_id}")
        
        # TODO: Use actual RL agent
        # For now, simple supply-demand logic
        
        mempool_size = features.get('mempool_size', 0)
        current_price = features.get('current_gas_price', 0.001)
        network_congestion = features.get('network_congestion', 0.5)
        
        # Increase price if lots of pending txs
        price_multiplier = 1.0 + (min(1.0, mempool_size / 10000) * 0.3)
        recommended_price = current_price * price_multiplier
        
        return {
            'type': 'prediction',
            'decision_id': decision_id,
            'phase': 4,
            'prediction': {
                'recommended_gas_price': float(recommended_price),
                'expected_confirmation_blocks': max(1, int(mempool_size / 1000)),
                'action': 'INCREASE' if price_multiplier > 1.1 else 'DECREASE' if price_multiplier < 0.9 else 'MAINTAIN',
                'reasoning': f'Mempool {mempool_size} txs, congestion {network_congestion:.2f}, price {recommended_price:.6f}'
            },
            'timestamp': datetime.now().isoformat()
        }

    def _handle_training_data(self, message: Dict) -> Dict:
        """
        Log training data for offline model improvement.
        v2.0: Actually appends feedback to CSV for future retraining.
        """
        decision_id = message.get('decision_id', 'unknown')
        phase = message.get('phase', 0)
        features = message.get('features', {})
        outcome = message.get('outcome', {})
        
        logger.info(f"Training data received for PHASE {phase}, decision {decision_id}")
        
        try:
            # Append feedback to a JSONL file for future retraining
            feedback_dir = Path(self.models_dir) / 'feedback'
            feedback_dir.mkdir(parents=True, exist_ok=True)
            
            feedback_file = feedback_dir / f'phase{phase}_feedback.jsonl'
            entry = {
                'decision_id': decision_id,
                'phase': phase,
                'features': features,
                'outcome': outcome,
                'timestamp': datetime.now(timezone.utc).isoformat()
            }
            
            import json as json_mod
            with open(feedback_file, 'a') as f:
                f.write(json_mod.dumps(entry) + '\n')
            
            logger.info(f"✓ Feedback saved to {feedback_file}")
        except Exception as e:
            logger.warning(f"Failed to save feedback: {e}")
        
        return {
            'type': 'acknowledgment',
            'decision_id': decision_id,
            'status': 'logged'
        }

    def _handle_health_check(self) -> Dict:
        """
        Respond to daemon health check with real model status.
        """
        phase1_status = 'not_loaded'
        phase2_status = 'not_loaded'
        
        if self.models.get('phase1_model') is not None:
            model_type = type(self.models['phase1_model']).__name__
            phase1_status = f'ready ({model_type})'
        
        if self.models.get('phase2_model') is not None:
            model_type = type(self.models['phase2_model']).__name__
            phase2_status = f'ready ({model_type})'
        
        return {
            'type': 'health_check_response',
            'status': 'healthy',
            'timestamp': datetime.now(timezone.utc).isoformat(),
            'use_trained_models': self.use_trained_models,
            'models': {
                'phase1_block_validator': phase1_status,
                'phase2_difficulty': phase2_status,
                'phase3_sybil': 'heuristic',
                'phase4_gas': 'heuristic'
            },
            'auto_retrain': {
                'enabled': True,
                'csv_rows': self._last_train_row_count,
                'threshold': self._retrain_threshold,
                'check_interval_s': self._retrain_interval
            }
        }

    def reload_models(self):
        """
        Hot-reload models from disk without restarting the service.
        Called periodically or when a RELOAD command is received.
        """
        logger.info("Hot-reloading models from disk...")
        try:
            self._load_models()
            logger.info("✓ Models reloaded successfully")
            return True
        except Exception as e:
            logger.error(f"✗ Model reload failed: {e}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description='NINA ML Service - AI microservice for blockchain optimization'
    )
    parser.add_argument('--host', default='127.0.0.1', help='Bind address')
    parser.add_argument('--port', type=int, default=5556, help='Listen port')
    parser.add_argument('--models-dir', default='./models', help='Directory with trained models')
    parser.add_argument('--loglevel', default='INFO', help='Log level')
    
    args = parser.parse_args()
    
    logger.setLevel(args.loglevel)
    
    service = NINAMLService(host=args.host, port=args.port, models_dir=args.models_dir)
    
    try:
        service.start()
    except KeyboardInterrupt:
        logger.info("Shutdown signal received")
        service.stop()
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
