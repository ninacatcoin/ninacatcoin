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
    
    print(f'\n{description}')
    print(f'  Hashrate Trend: {hashrate_trend_pct}%')
    print(f'  Block Time: {block_time_sec}s')
    print(f'  -> Action: {resp.get("action")}')
    print(f'  -> Multiplier: {resp.get("difficulty_multiplier")}')
    print(f'  -> Confidence: {resp.get("confidence")}')
    print(f'  -> Prediction: {resp.get("hashrate_analysis", {}).get("prediction")}')

if __name__ == '__main__':
    print('='*60)
    print('PHASE 2: Difficulty Optimization Tests')
    print('='*60)
    
    test_phase2(3.5, 115, 'Scenario 1: Growing hashrate, fast blocks')
    test_phase2(-2.0, 125, 'Scenario 2: Declining hashrate, slow blocks')
    test_phase2(0.5, 120, 'Scenario 3: Stable hashrate, perfect blocks')  
    test_phase2(5.0, 110, 'Scenario 4: Strong growth, very fast blocks')
    test_phase2(-5.0, 140, 'Scenario 5: Major decline, very slow blocks')
    
    print('\n' + '='*60)
    print('PHASE 2 Tests Complete')
    print('='*60)
