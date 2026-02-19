#!/usr/bin/env python3
"""
Quick test to verify trained models are working in server.py
"""

import socket
import json
import time

def test_phase1():
    """Test Phase 1 block validation"""
    print("\n" + "="*60)
    print("Testing PHASE 1: Block Validation with ML Model")
    print("="*60)
    
    # Test cases with different feature combinations
    test_cases = [
        {
            'name': 'High quality block',
            'data': {
                'phase': 'PHASE_1_BLOCK_VALIDATE',
                'decision_id': 'test_highquality_001',
                'network_health': 0.95,
                'miner_reputation': 0.85,
                'hash_entropy': 250,
            }
        },
        {
            'name': 'Normal block',
            'data': {
                'phase': 'PHASE_1_BLOCK_VALIDATE',
                'decision_id': 'test_normal_001',
                'network_health': 0.75,
                'miner_reputation': 0.65,
                'hash_entropy': 200,
            }
        },
        {
            'name': 'Suspicious block',
            'data': {
                'phase': 'PHASE_1_BLOCK_VALIDATE',
                'decision_id': 'test_suspicious_001',
                'network_health': 0.4,
                'miner_reputation': 0.3,
                'hash_entropy': 100,
            }
        },
    ]
    
    for test in test_cases:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('127.0.0.1', 5556))
            
            msg = json.dumps(test['data'])
            sock.sendall(msg.encode('utf-8'))
            
            # Receive response
            response_data = b''
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk
            
            sock.close()
            
            response = json.loads(response_data.decode('utf-8'))
            
            print(f"\n✓ {test['name']}:")
            print(f"  Input: health={test['data']['network_health']:.2f}, "
                  f"reputation={test['data']['miner_reputation']:.2f}, "
                  f"entropy={test['data']['hash_entropy']}")
            print(f"  Model used: {response.get('model', 'unknown')}")
            print(f"  Confidence: {response.get('confidence', 'N/A'):.4f}")
            print(f"  Action: {response.get('action', 'N/A')}")
            print(f"  Component: {response.get('components', {})}")
            
        except Exception as e:
            print(f"✗ Error: {e}")
    
    print()

def test_phase2():
    """Test Phase 2 difficulty optimization"""
    print("\n" + "="*60)
    print("Testing PHASE 2: Difficulty Optimization with ML Model")
    print("="*60)
    
    # Test cases with different hashrate conditions
    test_cases = [
        {
            'name': 'Growing hashrate',
            'data': {
                'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
                'decision_id': 'test_growing_001',
                'difficulty': 5000000,
                'block_time': 90,
                'hashrate': 1000,
                'hashrate_trend': 5.0,
                'target_block_time': 120,
                'avg_block_time': 115,
            }
        },
        {
            'name': 'Stable hashrate',
            'data': {
                'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
                'decision_id': 'test_stable_001',
                'difficulty': 5000000,
                'block_time': 120,
                'hashrate': 1000,
                'hashrate_trend': 0.0,
                'target_block_time': 120,
                'avg_block_time': 120,
            }
        },
        {
            'name': 'Declining hashrate',
            'data': {
                'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
                'decision_id': 'test_declining_001',
                'difficulty': 5000000,
                'block_time': 150,
                'hashrate': 800,
                'hashrate_trend': -3.5,
                'target_block_time': 120,
                'avg_block_time': 130,
            }
        },
    ]
    
    for test in test_cases:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('127.0.0.1', 5556))
            
            msg = json.dumps(test['data'])
            sock.sendall(msg.encode('utf-8'))
            
            # Receive response
            response_data = b''
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk
            
            sock.close()
            
            response = json.loads(response_data.decode('utf-8'))
            
            print(f"\n✓ {test['name']}:")
            print(f"  Input: block_time={test['data']['block_time']:.0f}s, "
                  f"hashrate_trend={test['data']['hashrate_trend']:.1f}%")
            print(f"  Model used: {response.get('model', 'unknown')}")
            print(f"  Multiplier: {response.get('difficulty_multiplier', 'N/A'):.4f}")
            print(f"  Action: {response.get('action', 'N/A')}")
            print(f"  Confidence: {response.get('confidence', 'N/A'):.4f}")
            print(f"  Recommended difficulty: {response.get('recommended_difficulty', 'N/A')}")
            
        except Exception as e:
            print(f"✗ Error: {e}")
    
    print()

if __name__ == '__main__':
    print("\n" + "="*60)
    print("TESTING TRAINED ML MODELS")
    print("="*60)
    
    # Give server time to start if needed
    time.sleep(0.5)
    
    test_phase1()
    test_phase2()
    
    print("\n" + "="*60)
    print("TESTING COMPLETE")
    print("="*60)
