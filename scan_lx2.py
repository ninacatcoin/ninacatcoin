import requests

RPC_URL = "http://127.0.0.1:19021/json_rpc"
START_BLOCK = 0
END_BLOCK   = 1000000000000

# ninacatcoin params (tomados de cryptonote_config.h)
COIN = 10 ** 12
MONEY_SUPPLY = (1 << 64) - 1  # uint64_max — infinite supply
EMISSION_SPEED_FACTOR = 20     # 21 - (target_minutes - 1)
FINAL_SUBSIDY = 800000000000   # 0.80 NINA per block (tail emission)
GENESIS_REWARD = 1_000_000 * COIN  # 1M NINA
X2_TARGET = 2.0
X2_TOLERANCE = 0.01
X200_TARGET = 200.0
PRINT_ALL = False

def get_top_height() -> int:
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_info",
        "params": {}
    }
    r = requests.post(RPC_URL, json=payload)
    data = r.json()
    if "error" in data:
        raise RuntimeError(f"RPC error get_info: {data['error']}")
    # get_info.height is the next block height; top block is height-1
    return max(0, data["result"]["height"] - 1)

def get_reward(height):
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_block_header_by_height",
        "params": {"height": height}
    }
    r = requests.post(RPC_URL, json=payload)
    data = r.json()
    if "error" in data:
        raise RuntimeError(f"RPC error get_block_header_by_height@{height}: {data['error']}")
    return data["result"]["block_header"]["reward"]

def format_coin(amount: int) -> str:
    return f"{amount / COIN:.8f}"

def get_coinbase_sum(height, count):
    payload = {
        "jsonrpc": "2.0",
        "id": "0",
        "method": "get_coinbase_tx_sum",
        "params": {"height": height, "count": count}
    }
    r = requests.post(RPC_URL, json=payload)
    data = r.json()
    if "error" in data:
        raise RuntimeError(f"RPC error get_coinbase_tx_sum@{height}: {data['error']}")
    return int(data["result"]["emission_amount"])

def compute_base_reward(height: int, already_generated: int) -> int:
    """Monero-style smooth emission: (MONEY_SUPPLY - already_generated) >> emission_speed_factor"""
    if height == 0:
        return GENESIS_REWARD
    base = (MONEY_SUPPLY - already_generated) >> EMISSION_SPEED_FACTOR
    if base < FINAL_SUBSIDY:
        base = FINAL_SUBSIDY
    return base

TOP_HEIGHT = get_top_height()
END_HEIGHT = min(END_BLOCK, TOP_HEIGHT)

already_generated = 0

for height in range(START_BLOCK, END_HEIGHT + 1):
    real_reward = get_reward(height)
    base_reward = compute_base_reward(height, already_generated)
    already_generated += real_reward

    if base_reward == 0:
        continue

    multiplier = real_reward / base_reward
    label = ""

    if abs(multiplier - X2_TARGET) < X2_TOLERANCE:
        label = "X2"

    elif multiplier >= X200_TARGET:
        label = "X200"

    if label or PRINT_ALL:
        tag = label if label else "ALL"
        print(f"[{tag}] bloque={height} pago={format_coin(real_reward)} NINA x{multiplier:.2f}")
