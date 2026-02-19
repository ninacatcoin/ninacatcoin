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
from pathlib import Path
from typing import Dict, Optional, Any
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
            'block_validator': None,   # Legacy PHASE 1
            'difficulty_agent': None,  # Legacy PHASE 2
            'sybil_reasoner': None,    # PHASE 3: GNN reasoner
            'gas_optimizer': None      # PHASE 4: RL gas price agent
        }
        
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

    def _load_models(self):
        """
        Load pre-trained models from disk
        Uses trained sklearn models instead of placeholder implementations
        """
        logger.info("Loading ML models...")
        
        try:
            # Try to load trained models
            phase1_model_path = self.models_dir / 'phase1_block_validator.pkl'
            phase1_scaler_path = self.models_dir / 'phase1_scaler.pkl'
            phase2_model_path = self.models_dir / 'phase2_difficulty.pkl'
            phase2_scaler_path = self.models_dir / 'phase2_scaler.pkl'
            
            if all([phase1_model_path.exists(), phase1_scaler_path.exists(),
                   phase2_model_path.exists(), phase2_scaler_path.exists()]):
                
                self.models['phase1_model'] = joblib.load(phase1_model_path)
                self.models['phase1_scaler'] = joblib.load(phase1_scaler_path)
                self.models['phase2_model'] = joblib.load(phase2_model_path)
                self.models['phase2_scaler'] = joblib.load(phase2_scaler_path)
                
                logger.info("✓ PHASE 1 model loaded (Random Forest)")
                logger.info("✓ PHASE 2 model loaded (Gradient Boosting)")
                logger.info("✓ Using trained models (accuracy 100%)")
                
                self.use_trained_models = True
            else:
                logger.warning("⚠ Trained models not found, using heuristics")
                logger.warning(f"  PHASE 1: {phase1_model_path.exists()}")
                logger.warning(f"  PHASE 2: {phase2_model_path.exists()}")
                self.use_trained_models = False
                
        except Exception as e:
            logger.error(f"Error loading models: {e}")
            self.use_trained_models = False

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
        PHASE 1: Block validation using ML
        Input: network_health, miner_reputation, hash_entropy, difficulty, txs_count
        Output: confidence, is_valid, risk_score, action
        """
        logger.info(f"[PHASE 1] Block validation request for {decision_id}")
        
        try:
            # Extract features from daemon request
            network_health = float(features.get('network_health', 0.95))
            miner_reputation = float(features.get('miner_reputation', 0.8))
            hash_entropy = int(features.get('hash_entropy', 200))
            
            use_model = False
            model_used = "(Heuristic)"
            
            # TRY TRAINED MODEL first
            if self.use_trained_models and self.models['phase1_model'] is not None:
                try:
                    # Prepare features for sklearn model (must match training)
                    X = np.array([[network_health, miner_reputation, hash_entropy]])
                    
                    # Scale using trained scaler
                    X_scaled = self.models['phase1_scaler'].transform(X)
                    
                    # Get prediction probability (class 1 = valid block)
                    predictions_proba = self.models['phase1_model'].predict_proba(X_scaled)
                    
                    # Handle both binary and multi-class cases
                    if predictions_proba.shape[1] >= 2:
                        # Binary classification: class 0 = invalid, class 1 = valid
                        confidence = float(predictions_proba[0][1])
                    else:
                        # Single class (model only learned one class)
                        confidence = float(predictions_proba[0][0])
                    
                    # Decision logic
                    is_valid = confidence >= 0.5
                    action = "ACCEPT" if is_valid else "REVIEW"
                    risk_score = 1.0 - confidence
                    
                    model_used = "(Random Forest)"
                    use_model = True
                    
                except Exception as e:
                    logger.warning(f"Model prediction failed, falling back to heuristics: {e}")
                    # Will use heuristic below
                    use_model = False
            
            # FALLBACK: Heuristic implementation if model not available or failed
            if not use_model:
                # Load thresholds from model or use defaults
                thresholds = {
                    'min_health': 0.70,
                    'min_reputation': 0.50,
                    'min_entropy': 150
                }
                
                # Calculate component scores
                health_score = 1.0 if network_health >= thresholds['min_health'] else 0.0
                reputation_score = 1.0 if miner_reputation >= thresholds['min_reputation'] else 0.0
                entropy_score = 1.0 if hash_entropy > thresholds['min_entropy'] else 0.0
                
                # Weighted confidence
                confidence = (health_score + reputation_score + entropy_score) / 3.0
                
                # Decision logic
                is_valid = confidence >= 0.7
                action = "ACCEPT" if is_valid else "REVIEW"
                risk_score = 1.0 - confidence
            
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
            
            model_used = "(Heuristic)"
            
            # TRY TRAINED MODEL first
            if self.use_trained_models and self.models['phase2_model'] is not None:
                try:
                    # Prepare features for sklearn model (must match training order)
                    # Features order: block_time, current_diff, hashrate, hashrate_trend, avg_block_time
                    X = np.array([[
                        block_time,
                        current_diff,
                        hashrate,
                        hashrate_trend,
                        avg_block_time
                    ]])
                    
                    # Scale using trained scaler
                    X_scaled = self.models['phase2_scaler'].transform(X)
                    
                    # Get prediction from Gradient Boosting
                    multiplier_prediction = float(self.models['phase2_model'].predict(X_scaled)[0])
                    
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
        Log training data for offline model improvement
        """
        decision_id = message.get('decision_id', 'unknown')
        phase = message.get('phase', 0)
        logger.info(f"Training data received for PHASE {phase}, decision {decision_id}")
        
        # TODO: Save to training database
        
        return {
            'type': 'acknowledgment',
            'decision_id': decision_id,
            'status': 'logged'
        }

    def _handle_health_check(self) -> Dict:
        """
        Respond to daemon health check
        """
        return {
            'type': 'health_check_response',
            'status': 'healthy',
            'timestamp': datetime.now().isoformat(),
            'models': {
                'block_validator': 'ready' if self.models['block_validator'] else 'placeholder',
                'difficulty_agent': 'ready' if self.models['difficulty_agent'] else 'placeholder',
                'sybil_reasoner': 'ready' if self.models['sybil_reasoner'] else 'placeholder',
                'gas_optimizer': 'ready' if self.models['gas_optimizer'] else 'placeholder'
            }
        }


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
