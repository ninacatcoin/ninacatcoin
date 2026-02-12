#!/usr/bin/env python3
import json
import os
import sys
import time
import urllib.request
import urllib.error

MASK_64 = (1 << 64) - 1

KECCAK_RC = [
    0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
    0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
    0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
]

KECCAK_ROT = [
    [0, 36, 3, 41, 18],
    [1, 44, 10, 45, 2],
    [62, 6, 43, 15, 61],
    [28, 55, 25, 21, 56],
    [27, 20, 39, 8, 14],
]


def rotl64(x, n):
    return ((x << n) | (x >> (64 - n))) & MASK_64


def keccak_f1600(state):
    for rc in KECCAK_RC:
        c = [state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20] for x in range(5)]
        d = [c[(x - 1) % 5] ^ rotl64(c[(x + 1) % 5], 1) for x in range(5)]
        for x in range(5):
            for y in range(5):
                state[x + 5 * y] ^= d[x]

        b = [0] * 25
        for x in range(5):
            for y in range(5):
                b[y + 5 * ((2 * x + 3 * y) % 5)] = rotl64(state[x + 5 * y], KECCAK_ROT[x][y])

        for x in range(5):
            for y in range(5):
                state[x + 5 * y] = b[x + 5 * y] ^ ((~b[(x + 1) % 5 + 5 * y]) & b[(x + 2) % 5 + 5 * y])

        state[0] ^= rc


def keccak_256(data):
    rate = 136
    state = [0] * 25

    pad_len = rate - (len(data) % rate)
    padded = data + b"\x01" + b"\x00" * (pad_len - 2) + b"\x80"

    for offset in range(0, len(padded), rate):
        block = padded[offset:offset + rate]
        for i in range(rate // 8):
            state[i] ^= int.from_bytes(block[i * 8:(i + 1) * 8], "little")
        keccak_f1600(state)

    out = bytearray()
    while len(out) < 32:
        for i in range(rate // 8):
            out.extend(state[i].to_bytes(8, "little"))
            if len(out) >= 32:
                break
        if len(out) < 32:
            keccak_f1600(state)
    return bytes(out[:32])


def env_int(name, default):
    value = os.getenv(name)
    return int(value) if value is not None else default


RPC_URL = os.getenv("NINACATCOIN_RPC_URL", "http://127.0.0.1:19081/json_rpc")
WEBHOOK_URL = os.getenv("NINACATCOIN_DISCORD_WEBHOOK", "").strip()
USER_AGENT = "NinacatcoinBot/1.0"
POLL_SECONDS = env_int("NINACATCOIN_POLL_SECONDS", 15)
STATE_FILE = os.getenv("NINACATCOIN_STATE_FILE", "discord_lottery_bot.state.json")
POST_ALL = os.getenv("NINACATCOIN_POST_ALL", "0").lower() in {"1", "true", "yes"}
SELF_RPC = os.getenv("NINACATCOIN_SELF_RPC", "").strip() or RPC_URL
FAILOVER_RPCS = [
    item for item in (s.strip() for s in os.getenv("NINACATCOIN_FAILOVER_RPCS", "").split(",")) if item
]
FAILOVER_CHECK_SECONDS = env_int("NINACATCOIN_FAILOVER_CHECK_SECONDS", 30)
BACKFILL_FROM = env_int("NINACATCOIN_BACKFILL_FROM", -1)
BACKFILL_TO = env_int("NINACATCOIN_BACKFILL_TO", -1)
EXPLORER_URL = os.getenv("NINACATCOIN_EXPLORER_URL", "").strip()  # e.g., https://ninacatcoin.es/explorer/

COIN = env_int("NINACATCOIN_COIN", 1000000000000)
DISPLAY_DECIMALS = env_int("NINACATCOIN_DISPLAY_DECIMALS", 12)
MONEY_SUPPLY = env_int("NINACATCOIN_MONEY_SUPPLY", 1000000000000 * COIN)
FINAL_BRAKE_REMAINING = env_int("NINACATCOIN_FINAL_BRAKE_REMAINING", 10000 * COIN)
BLOCKS_PER_YEAR = env_int("NINACATCOIN_BLOCKS_PER_YEAR", 262800)
X2_TIMES_PER_YEAR = env_int("NINACATCOIN_X2_TIMES_PER_YEAR", 183)
X200_TIMES_PER_YEAR = env_int("NINACATCOIN_X200_TIMES_PER_YEAR", 6)
GENESIS_REWARD = env_int("NINACATCOIN_GENESIS_REWARD", 100000 * COIN)
BASE_BLOCK_REWARD = env_int("NINACATCOIN_BASE_BLOCK_REWARD", 16 * COIN)
MIN_BLOCK_REWARD = env_int("NINACATCOIN_MIN_BLOCK_REWARD", 2 * COIN)
HALVING_INTERVAL_BLOCKS = env_int("NINACATCOIN_HALVING_INTERVAL_BLOCKS", 262800)
X2_START_HEIGHT = env_int("NINACATCOIN_X2_START_HEIGHT", 100)


def rpc_call_url(url, method, params=None, timeout=10):
    payload = {"jsonrpc": "2.0", "id": "0", "method": method}
    if params is not None:
        payload["params"] = params
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8")
    reply = json.loads(body)
    if "error" in reply:
        raise RuntimeError(reply["error"])
    return reply["result"]


def rpc_call(method, params=None):
    return rpc_call_url(RPC_URL, method, params=params, timeout=10)


def post_webhook(content):
    if not WEBHOOK_URL:
        print("Missing NINACATCOIN_DISCORD_WEBHOOK; skipping webhook post.")
        return
    payload = json.dumps({"content": content}).encode("utf-8")
    req = urllib.request.Request(
        WEBHOOK_URL,
        data=payload,
        headers={"Content-Type": "application/json", "User-Agent": USER_AGENT},
    )
    with urllib.request.urlopen(req, timeout=10):
        pass


def is_rpc_available(url):
    try:
        rpc_call_url(url, "get_block_count", timeout=5)
        return True
    except (urllib.error.URLError, RuntimeError):
        return False


def select_leader():
    if not FAILOVER_RPCS:
        return SELF_RPC
    for url in FAILOVER_RPCS:
        if is_rpc_available(url):
            return url
    return None


def format_coin(amount):
    return f"{amount / COIN:.{DISPLAY_DECIMALS}f}"


def get_block_count():
    result = rpc_call("get_block_count")
    return int(result["count"])


def get_block_header(height):
    result = rpc_call("get_block_header_by_height", {"height": height})
    return result["block_header"]


def get_coinbase_sum(height, count):
    result = rpc_call("get_coinbase_tx_sum", {"height": height, "count": count})
    return int(result["emission_amount"]), int(result["fee_amount"])


def event_roll(prev_hash_hex, tag):
    prev_bytes = bytes.fromhex(prev_hash_hex)
    data = prev_bytes + bytes([tag])
    h = keccak_256(data)
    return int.from_bytes(h[:8], "little")


def classify_event(height, prev_hash_hex, already_generated):
    remaining = MONEY_SUPPLY - already_generated
    events_allowed = remaining > FINAL_BRAKE_REMAINING
    is_x2 = False
    is_x200 = False

    if events_allowed and height >= X2_START_HEIGHT:
        if height == X2_START_HEIGHT:
            is_x2 = True
        else:
            rnd2 = event_roll(prev_hash_hex, 2)
            if (rnd2 % BLOCKS_PER_YEAR) < X2_TIMES_PER_YEAR:
                is_x2 = True

    if events_allowed:
        rnd200 = event_roll(prev_hash_hex, 200)
        if (rnd200 % BLOCKS_PER_YEAR) < X200_TIMES_PER_YEAR:
            is_x200 = True

    return is_x2, is_x200


def load_state():
    if not os.path.exists(STATE_FILE):
        return None
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return None


def save_state(last_height, already_generated, backfill_done=False):
    data = {
        "last_height": last_height,
        "already_generated_coins": already_generated,
        "backfill_done": backfill_done,
    }
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f)


def init_state():
    count = get_block_count()
    top_height = count - 1
    emission, _fees = get_coinbase_sum(0, top_height + 1)
    state = {
        "last_height": top_height,
        "already_generated_coins": emission,
        "backfill_done": False,
    }
    save_state(state["last_height"], state["already_generated_coins"], state["backfill_done"])
    print(f"Initialized at height {top_height} with emission {emission}.")
    return state


def get_base_reward(height):
    """Calculate base block reward without multipliers"""
    if height == 0:
        return GENESIS_REWARD
    halvings = height // HALVING_INTERVAL_BLOCKS
    base_reward = BASE_BLOCK_REWARD >> halvings
    if base_reward < MIN_BLOCK_REWARD:
        base_reward = MIN_BLOCK_REWARD
    return base_reward


def validate_event_payout(header, is_x2, is_x200, already_generated, emission):
    """Verify that block actually paid X2 or X200"""
    base_reward = get_base_reward(header["height"])
    expected_reward = base_reward
    if is_x2:
        expected_reward *= 2
    if is_x200:
        expected_reward *= 200
    remaining = MONEY_SUPPLY - already_generated
    if remaining > 0 and expected_reward > remaining:
        expected_reward = remaining
    if emission != expected_reward:
        tag = "X2+X200" if is_x2 and is_x200 else ("X2" if is_x2 else "X200")
        print(f"{tag} event at {header['height']}: expected {expected_reward}, got {emission}")
        return False
    return True


def build_message(header, emission, fees, is_x2, is_x200):
    height = header["height"]
    reward = header["reward"]
    difficulty = header["difficulty"]
    block_hash = header["hash"]
    timestamp = header.get("timestamp", 0)
    prev_hash = header.get("prev_hash", "")
    miner_address = header.get("miner_tx_public_key", "unknown")  # Reward address

    tags = []
    if is_x2:
        tags.append("x2")
    if is_x200:
        tags.append("x200")
    tag_str = ", ".join(tags) if tags else "normal"

    lines = [
        f"🎉 Ninacatcoin block {height} ({tag_str})",
        f"",
        f"**Block Info:**",
        f"Hash: `{block_hash}`",
        f"Previous: `{prev_hash}`",
        f"",
        f"**Rewards:**",
        f"Block Reward: {format_coin(reward)} NIA",
        f"Total Emission: {format_coin(emission)} NIA",
        f"Fees: {format_coin(fees)} NIA",
        f"",
        f"**Network:**",
        f"Difficulty: {difficulty}",
        f"Timestamp: {timestamp}",
        f"Miner Address: `{miner_address}`",
    ]
    
    # Add explorer link if configured
    if EXPLORER_URL:
        explorer_link = f"{EXPLORER_URL}/block/{block_hash}"
        lines.append(f"")
        lines.append(f"🔗 [View on Explorer]({explorer_link})")
    
    return "\n".join(lines)


def run_backfill(start_height, end_height, already_generated, active):
    if end_height < start_height:
        print(f"Backfill skipped (end {end_height} < start {start_height}).")
        return already_generated

    print(f"Backfill scan {start_height}..{end_height}")
    for height in range(start_height, end_height + 1):
        header = get_block_header(height)
        prev_hash = header.get("prev_hash")
        if not prev_hash:
            print(f"Missing prev_hash at height {height}; skipping.")
            continue

        is_x2, is_x200 = classify_event(height, prev_hash, already_generated)
        emission, fees = get_coinbase_sum(height, 1)
        already_generated_before = already_generated
        already_generated += emission

        if active:
            if is_x2 or is_x200:
                # Validate that block actually paid X2 or X200
                if validate_event_payout(header, is_x2, is_x200, already_generated_before, emission):
                    message = build_message(header, emission, fees, is_x2, is_x200)
                    post_webhook(message)
            elif POST_ALL:
                message = build_message(header, emission, fees, False, False)
                post_webhook(message)

    return already_generated


def main():
    state = load_state()
    if state is None:
        state = init_state()

    last_height = int(state["last_height"])
    already_generated = int(state["already_generated_coins"])
    backfill_done = bool(state.get("backfill_done", False))
    backfill_pending = BACKFILL_FROM >= 0 and not backfill_done
    leader = None
    last_leader_check = 0.0
    active = True

    if FAILOVER_RPCS and SELF_RPC not in FAILOVER_RPCS:
        print("Warning: NINACATCOIN_SELF_RPC is not in NINACATCOIN_FAILOVER_RPCS. Bot will stay passive.")

    while True:
        try:
            now = time.time()
            if now - last_leader_check >= FAILOVER_CHECK_SECONDS:
                leader = select_leader()
                last_leader_check = now
                prev_active = active
                active = (leader == SELF_RPC) if FAILOVER_RPCS else True
                if active != prev_active:
                    role = "active" if active else "standby"
                    print(f"Bot role: {role} (leader={leader})")

            count = get_block_count()
            top_height = count - 1

            if backfill_pending and active:
                end_height = BACKFILL_TO if BACKFILL_TO >= 0 else top_height
                start_height = max(0, min(BACKFILL_FROM, end_height))
                emission_before, _fees = get_coinbase_sum(0, start_height)
                already_generated = run_backfill(start_height, end_height, emission_before, active)
                emission_now, _fees = get_coinbase_sum(0, top_height + 1)
                last_height = top_height
                already_generated = emission_now
                backfill_pending = False
                backfill_done = True
                save_state(last_height, already_generated, backfill_done)
                print(f"Backfill done; state set to height {last_height}.")

            if top_height > last_height:
                for height in range(last_height + 1, top_height + 1):
                    header = get_block_header(height)
                    prev_hash = header.get("prev_hash")
                    if not prev_hash:
                        print(f"Missing prev_hash at height {height}; skipping.")
                        continue

                    is_x2, is_x200 = classify_event(height, prev_hash, already_generated)
                    emission, fees = get_coinbase_sum(height, 1)
                    already_generated_before = already_generated
                    already_generated += emission

                    # Only post if event is confirmed by actual reward
                    if active:
                        if is_x2 or is_x200:
                            # Validate that block actually paid X2 or X200
                            if validate_event_payout(header, is_x2, is_x200, already_generated_before, emission):
                                message = build_message(header, emission, fees, is_x2, is_x200)
                                post_webhook(message)
                        elif POST_ALL:
                            message = build_message(header, emission, fees, False, False)
                            post_webhook(message)

                    last_height = height
                    save_state(last_height, already_generated, backfill_done)

            time.sleep(POLL_SECONDS)
        except (urllib.error.URLError, RuntimeError, OSError) as exc:
            print(f"Error: {exc}")
            time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    if not WEBHOOK_URL:
        print("NINACATCOIN_DISCORD_WEBHOOK is not set. Set it and rerun.")
        sys.exit(1)
    main()
