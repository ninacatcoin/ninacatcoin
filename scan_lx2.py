import requests

RPC_URL = "http://127.0.0.1:19081/json_rpc"
START_BLOCK = 0
END_BLOCK   = 1000000000000

# ninacatcoin params (tomados de cryptonote_config.h)
# ninacatcoin uses 12 decimal places
COIN = 10 ** 12
GENESIS_REWARD = 10000 * COIN
BASE_BLOCK_REWARD = 4 * COIN
ninacatcoin_MIN_BLOCK_REWARD = 2 * COIN
ninacatcoin_HALVING_INTERVAL_BLOCKS = 262_800
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

def compute_base_reward(height: int) -> int:
    if height == 0:
        return GENESIS_REWARD

    halvings = height // ninacatcoin_HALVING_INTERVAL_BLOCKS
    base = BASE_BLOCK_REWARD >> halvings
    if base < ninacatcoin_MIN_BLOCK_REWARD:
        base = ninacatcoin_MIN_BLOCK_REWARD
    return base

TOP_HEIGHT = get_top_height()
END_HEIGHT = min(END_BLOCK, TOP_HEIGHT)

for height in range(START_BLOCK, END_HEIGHT + 1):
    real_reward = get_reward(height)
    base_reward = compute_base_reward(height)

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
