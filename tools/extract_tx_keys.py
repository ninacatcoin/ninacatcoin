#!/usr/bin/env python3
"""
Extract RingCT key pairs from a ninacatcoin transaction for use in unit tests
"""

import json
import requests
import struct
import sys

# Transaction ID to extract
TX_ID = "23487a6ca0a7b7a804fb2d7121b0f8b6bd3850d030cfc58e56a328b4409023e2"

# RPC endpoint
RPC_URL = "http://127.0.0.1:29081"

def get_transaction(tx_hash):
    """Fetch transaction from daemon RPC"""
    # Try REST endpoint first
    try:
        payload = {
            "txs_hashes": [tx_hash]
        }
        
        response = requests.post(RPC_URL + "/get_transactions", json=payload, timeout=5)
        result = response.json()
        
        if "txs" in result:
            return result["txs"][0]
        elif "error" in result:
            print(f"Error: {result['error']}")
            return None
        else:
            print(f"Response: {result}")
            return None
    except Exception as e:
        print(f"Connection error: {e}")
        print(f"Make sure ninacatcoind is running at {RPC_URL}")
        return None

def extract_keys_from_tx(tx_json):
    """Extract destination and mask keys from transaction"""
    try:
        tx_data = json.loads(tx_json) if isinstance(tx_json, str) else tx_json
        
        # RingCT signatures are in rct_signatures
        if "rct_signatures" not in tx_data:
            print("Error: No RingCT signatures found")
            return None
        
        rct_sig = tx_data["rct_signatures"]
        
        # The mixRing contains the ring members (destination, mask pairs)
        if "mixRing" not in rct_sig:
            print("Error: No mixRing found in RingCT signatures")
            return None
        
        mix_ring = rct_sig["mixRing"]
        
        # Extract all keys from the first input's ring
        if len(mix_ring) == 0:
            print("Error: mixRing is empty")
            return None
        
        first_ring = mix_ring[0]
        
        print(f"Found {len(first_ring)} ring members")
        
        keys = []
        for i, member in enumerate(first_ring):
            dest = member.get("dest", "")
            mask = member.get("mask", "")
            
            if dest and mask:
                keys.append((dest, mask))
                print(f"  [{i}] dest: {dest[:16]}... mask: {mask[:16]}...")
        
        return keys
    
    except Exception as e:
        print(f"Error parsing transaction: {e}")
        import traceback
        traceback.print_exc()
        return None

def format_for_cpp(keys):
    """Format keys as C++ code"""
    if not keys:
        return None
    
    cpp_lines = ["static const rct::ctkeyM tx1_input_pubkeys =", "{{"]
    
    for i, (dest, mask) in enumerate(keys):
        comma = "," if i < len(keys) - 1 else ""
        cpp_lines.append(f'    make_ctkey("{dest}", "{mask}"){comma}')
    
    cpp_lines.append("}};")
    
    return "\n".join(cpp_lines)

def main():
    print(f"Extracting keys from transaction: {TX_ID}")
    print(f"Connecting to RPC at: {RPC_URL}")
    print()
    
    # Get transaction
    print("Fetching transaction...")
    tx_json = get_transaction(TX_ID)
    
    if not tx_json:
        print("Failed to fetch transaction")
        return False
    
    print(f"Transaction retrieved")
    print()
    
    # Extract keys
    print("Extracting ring member keys...")
    keys = extract_keys_from_tx(tx_json)
    
    if not keys:
        print("Failed to extract keys")
        return False
    
    print(f"\nSuccessfully extracted {len(keys)} key pairs")
    print()
    
    # Format for C++
    cpp_code = format_for_cpp(keys)
    
    print("C++ Code:")
    print("=" * 80)
    print(cpp_code)
    print("=" * 80)
    
    # Save to file
    output_file = "extracted_keys.cpp"
    with open(output_file, "w") as f:
        f.write(cpp_code)
    
    print(f"\nKeys saved to: {output_file}")
    
    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
