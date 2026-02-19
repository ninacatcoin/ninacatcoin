import socket
import json

def test_phase2(hashrate_trend_pct, block_time_sec, description):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 5556))
    
    msg = {
        'phase': 'PHASE_2_DIFFICULTY_OPTIMIZE',
        'decision_id': f'phase2_{hashrate_trend_pct}_{block_time_sec}',
        'difficulty': 13500000,
        'block_time': block_time_sec,
        'hashrate': 1000000,
        'hashrate_trend': hashrate_trend_pct,
        'target_block_time': 120
    }
    
    s.sendall((json.dumps(msg) + '\n').encode('utf-8'))
    resp = json.loads(s.recv(4096).decode('utf-8'))
    s.close()
    
    primary = resp.get('lwma_complement', {}).get('primary_signal', 'N/A')
    trend = resp.get('lwma_complement', {}).get('trend_signal', 'N/A')
    
    print(f'\n{description}')
    print(f'  Hashrate: {hashrate_trend_pct:+.1f}% | Block Time: {block_time_sec}s')
    print(f'  Primary: {primary:8} | Trend: {trend:8} | Action: {resp.get("action")}')
    print(f'  Multiplier: {resp.get("difficulty_multiplier"):6} | Confidence: {resp.get("confidence")}')

if __name__ == '__main__':
    print('='*80)
    print('PHASE 2: Extended Difficulty Optimization Test Suite')
    print('='*80)
    
    # Basic scenarios
    test_phase2(0.0, 120.0, '1. Neutral - Perfect conditions')
    test_phase2(2.5, 120.0, '2. Slight growth, perfect time')
    test_phase2(-2.5, 120.0, '3. Slight decline, perfect time')
    
    # Fast blocks
    test_phase2(0.0, 110.0, '4. Fast blocks, stable hashrate')
    test_phase2(3.0, 110.0, '5. Fast blocks with growing hashrate')
    test_phase2(-3.0, 110.0, '6. Fast blocks with declining hashrate')
    
    # Slow blocks
    test_phase2(0.0, 130.0, '7. Slow blocks, stable hashrate')
    test_phase2(3.0, 130.0, '8. Slow blocks with growing hashrate')
    test_phase2(-3.0, 130.0, '9. Slow blocks with declining hashrate')
    
    # Extreme cases
    test_phase2(7.0, 105.0, '10. EXTREME: +7% growth, very fast (105s)')
    test_phase2(-7.0, 135.0, '11. EXTREME: -7% decline, very slow (135s)')
    test_phase2(5.0, 95.0, '12. EXTREME: +5% growth, ultra-fast (95s)')
    test_phase2(-5.0, 145.0, '13. EXTREME: -5% decline, ultra-slow (145s)')
    
    print('\n' + '='*80)
    print('Extended PHASE 2 Tests Complete')
    print('='*80)
