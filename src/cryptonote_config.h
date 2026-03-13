// Copyright (c) 2026, The ninacatcoin Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#define CRYPTONOTE_DNS_TIMEOUT_MS                       20000

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_TX_SIZE                          1000000
#define CRYPTONOTE_MAX_TX_PER_BLOCK                     0x10000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            60  // legacy unlock window (fallback, not used with v18 from genesis)
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW_V18         30  // v18 blocks (height 0+): 30 blocks × 120s = 1 hour
#define CURRENT_TRANSACTION_VERSION                     2
#define CURRENT_BLOCK_MAJOR_VERSION                     18
#define CURRENT_BLOCK_MINOR_VERSION                     18
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2
#define CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE             10

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               30  // 30 blocks × 120s = 1 hour (same real time as Monero 60×60s)

// MONEY_SUPPLY - total number coins to be generated
// Monero-style smooth emission: base_reward = (MONEY_SUPPLY - already_generated) >> emission_speed_factor
// Tail emission kicks in when base_reward < FINAL_SUBSIDY → 0.80 NINA/block forever
#define MONEY_SUPPLY                                    ((uint64_t)(-1))
#define EMISSION_SPEED_FACTOR_PER_MINUTE                (21)
#define FINAL_SUBSIDY_PER_MINUTE                        ((uint64_t)400000000000) // 4 * pow(10, 11)

#define GENESIS_REWARD                                  ((uint64_t)1000000 * COIN) // 1M NINA to spendable wallet (block 0)

#define BLOCKS_PER_YEAR                                 262800
#define X2_TIMES_PER_YEAR                               183
#define X200_TIMES_PER_YEAR                             6

// ─── NINA Event System ──────────────────────────────────────────────
// Event seed = prev_id (previous block hash).
// Events multiply the base_reward for the miner:
//   X2  →  base_reward × 2
//   X200 → base_reward × 200
//
#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2    60000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1    20000 //size of block (bytes) after which reward for block calculated using block size - before first fork
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5    300000 //size of block (bytes) after which reward for block calculated using block size - second change, from v5
#define CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE   100000 // size in blocks of the long term block weight median window
#define CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR 50
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                12
// COIN - number of smallest units in one coin
#define COIN                                            ((uint64_t)1000000000000) // pow(10, 12)

// ─── NINACATCOIN FEE CALIBRATION (Monero-style emission) ────────────────
// Dynamic fees scale with block reward (Monero formula).
// Initial base_reward ~17.59 NINA, tail emission 0.80 NINA/block.
//
#define FEE_PER_KB_OLD                                  ((uint64_t)10000000000)   // pow(10, 10) — Monero default
#define FEE_PER_KB                                      ((uint64_t)2000000000)    // 2 * pow(10, 9) — Monero default
#define FEE_PER_BYTE                                    ((uint64_t)300000)
#define DYNAMIC_FEE_PER_KB_BASE_FEE                     ((uint64_t)2000000000)    // 2 * pow(10, 9) — Monero default
#define DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD            ((uint64_t)10000000000000) // 10 * pow(10, 12) — Monero default
#define DYNAMIC_FEE_PER_KB_BASE_FEE_V5                  ((uint64_t)DYNAMIC_FEE_PER_KB_BASE_FEE * (uint64_t)CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5)
#define DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT         ((uint64_t)3000)

#define ORPHANED_BLOCKS_MAX_COUNT                       100


#define DIFFICULTY_TARGET_V2                            120  // seconds
#define DIFFICULTY_TARGET_V1                            120  // seconds - before first fork
#define DIFFICULTY_WINDOW                               60  // blocks (reduced from 720 for faster reaction)
#define DIFFICULTY_LAG                                  2   // (reduced from 15, proportional to window)
#define DIFFICULTY_CUT                                  6   // timestamps to cut after sorting (reduced from 60)
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG

// NINACOIN: Emergency difficulty reset - not needed (v18 from genesis)
#define DIFFICULTY_RESET_HEIGHT                          0

// NINACOIN: Emergency Difficulty Adjustment (EDA)
// If the last block took more than EDA_THRESHOLD * target seconds,
// cap difficulty to match the observed hashrate. Prevents network stall
// when a large miner suddenly departs.
#define DIFFICULTY_EDA_THRESHOLD                         6   // trigger when block > 6 * 120s = 720s

// NINACOIN: NINA Local Difficulty Assist — active from genesis
// Deterministic local hashrate-trend correction (±5% max)
// that complements LWMA by reacting faster to sudden hashrate changes.
// Uses only blockchain data (timestamps/difficulties) — fully deterministic.
#define NINA_LOCAL_FORK_HEIGHT                           0
#define NINA_LOCAL_RECENT_WINDOW                         5   // blocks for recent average
#define NINA_LOCAL_OLDER_WINDOW                          15  // blocks for baseline average
#define NINA_LOCAL_SMOOTHING                             0.3 // dampening factor (0-1)
#define NINA_LOCAL_MAX_ADJUST                            0.05 // max ±5% adjustment

// ─── NINA AI LLM Model Configuration ────────────────────────────────────
// SHA-256 hash of the official NINA GGUF model file.
// Compiled into the binary — verified at daemon startup.
// Llama-3.2-3B-Instruct QLoRA r32/a64 → Q4_K_M (1.88 GB, 4.5 BPW)
// Generated: nina-llama3.2-3b-Q4_K_M.gguf  (v2 — 3074 examples, 2026-03-01)
#define NINA_MODEL_HASH "5c7be16cf1cc6a6a4e46e5adb85acf684af228abd10ef70178e81f95d7562ca9"

// Model file name (expected in data_dir)
#define NINA_MODEL_FILENAME "nina_model.gguf"

// Model download URLs (tried in order; SHA-256 verified after download)
#define NINA_MODEL_DOWNLOAD_URL_PRIMARY   "https://huggingface.co/ninacatcoin/nina-model/resolve/main/nina-3b-nina-ft-Q4_K_M.gguf"
#define NINA_MODEL_DOWNLOAD_URL_FALLBACK  "https://github.com/ninacatcoin/ninacatcoin/releases/download/v0.18/nina-3b-nina-ft-Q4_K_M.gguf"

// Model size bounds (sanity check before loading)
#define NINA_MODEL_MIN_SIZE_BYTES  (100ULL * 1024 * 1024)     // 100 MB minimum
#define NINA_MODEL_MAX_SIZE_BYTES  (4ULL * 1024 * 1024 * 1024) // 4 GB maximum

// Coinbase tags for NINA state embedding
#define NINA_COINBASE_TAG_MODEL_HASH  0xCA  // Model SHA-256 in coinbase extra
#define NINA_COINBASE_TAG_STATE       0xCB  // NINA state snapshot in coinbase extra

// LLM inference settings (CPU-only for consensus determinism)
#define NINA_LLM_N_GPU_LAYERS   0      // Must be 0 for deterministic consensus
#define NINA_LLM_N_CTX          2048   // Context window
#define NINA_LLM_N_THREADS      2      // Max threads for inference (sandboxed)
#define NINA_LLM_TEMP           0.0f   // Temperature 0 = deterministic output

// llama-server HTTP bridge (for non-consensus advisory queries)
#define NINA_LLAMA_SERVER_HOST  "127.0.0.1"
#define NINA_LLAMA_SERVER_PORT  8080


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1   DIFFICULTY_TARGET_V1 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2   DIFFICULTY_TARGET_V2 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1


#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET_V1 //just alias; used by tests


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_IDS_SYNCHRONIZING_MAX_COUNT              25000  //max blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT_PRE_V4       100    //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              20     //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_MAX_COUNT                  2048   //must be a power of 2, greater than 128, equal to SEEDHASH_EPOCH_BLOCKS

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                    (86400*3) //seconds, three days
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME     604800 //seconds, one week


// ─── NINACATCOIN DANDELION++ (differentiated from Monero) ───────────────
// Different parameters prevent cross-chain timing correlation attacks.
// Longer epochs and lower fluff probability = stronger stem privacy.
//
#define CRYPTONOTE_DANDELIONPP_STEMS              3 // 3 stem connections (Monero: 2) — harder to trace
#define CRYPTONOTE_DANDELIONPP_FLUFF_PROBABILITY 15 // 15% (Monero: 20%) — longer stem phase
#define CRYPTONOTE_DANDELIONPP_MIN_EPOCH         15 // 15 min (Monero: 10) — less frequent epoch rotation
#define CRYPTONOTE_DANDELIONPP_EPOCH_RANGE       45 // 45 sec (Monero: 30) — more epoch jitter
#define CRYPTONOTE_DANDELIONPP_FLUSH_AVERAGE      7 // 7 sec (Monero: 5) — more delayed fluff
#define CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE   50 // 50 sec (Monero: 39) — longer embargo window

// see src/cryptonote_protocol/levin_notify.cpp
#define CRYPTONOTE_NOISE_MIN_EPOCH                      5      // minutes
#define CRYPTONOTE_NOISE_EPOCH_RANGE                    30     // seconds
#define CRYPTONOTE_NOISE_MIN_DELAY                      10     // seconds
#define CRYPTONOTE_NOISE_DELAY_RANGE                    5      // seconds
#define CRYPTONOTE_NOISE_BYTES                          3*1024 // 3 KiB
#define CRYPTONOTE_NOISE_CHANNELS                       2      // Max outgoing connections per zone used for noise/covert sending

// Both below are in seconds. The idea is to delay forwarding from i2p/tor
// to ipv4/6, such that 2+ incoming connections _could_ have sent the tx
#define CRYPTONOTE_FORWARD_DELAY_BASE (CRYPTONOTE_NOISE_MIN_DELAY + CRYPTONOTE_NOISE_DELAY_RANGE)
#define CRYPTONOTE_FORWARD_DELAY_AVERAGE (CRYPTONOTE_FORWARD_DELAY_BASE + (CRYPTONOTE_FORWARD_DELAY_BASE / 2))

#define CRYPTONOTE_MAX_FRAGMENTS                        20 // ~20 * NOISE_BYTES max payload size for covert/noise send

#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT     1000
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_TX_COUNT        20000
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP       3
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP      25
#define DEFAULT_RPC_MAX_CONNECTIONS                     100
#define DEFAULT_RPC_SOFT_LIMIT_SIZE                     25 * 1024 * 1024 // 25 MiB
#define MAX_RPC_CONTENT_LENGTH                          1048576 // 1 MB

// =====================================================================
// NinaCatCoin P2P Network Parameters
// Intentionally different from Monero to fingerprint spy tools that
// assume Monero defaults (60s sync, 250 peers, 1000 whitelist).
// For large network mode (100,000+ nodes): uncomment NINACATCOIN_LARGE_NETWORK
// =====================================================================

// Uncomment the next line when network exceeds 100,000 nodes:
// #define NINACATCOIN_LARGE_NETWORK

#ifdef NINACATCOIN_LARGE_NETWORK
  // --- LARGE NETWORK MODE (100,000+ nodes) ---
  #define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  2000
  #define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   10000
  #define P2P_DEFAULT_CONNECTIONS_COUNT                   16
  #define P2P_DEFAULT_HANDSHAKE_INTERVAL                  45           //seconds
  #define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
  #define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  150
  #define P2P_MAX_PEERS_IN_HANDSHAKE                      150
  #define P2P_DEFAULT_CONNECTION_TIMEOUT                  3000       //3 seconds
  #define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               45         // seconds
  #define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
  #define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
  #define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
  #define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
  #define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            4
  #define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       4
  #define P2P_DEFAULT_LIMIT_RATE_UP                       16384      // kB/s
  #define P2P_DEFAULT_LIMIT_RATE_DOWN                     65536      // kB/s
#else
  // --- STANDARD MODE (current network size) ---
  #define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  500
  #define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   2500
  #define P2P_DEFAULT_CONNECTIONS_COUNT                   8
  #define P2P_DEFAULT_HANDSHAKE_INTERVAL                  45           //seconds
  #define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
  #define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  100
  #define P2P_MAX_PEERS_IN_HANDSHAKE                      100
  #define P2P_DEFAULT_CONNECTION_TIMEOUT                  3000       //3 seconds
  #define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               45         // seconds
  #define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
  #define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
  #define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
  #define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
  #define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            2
  #define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       2
  #define P2P_DEFAULT_LIMIT_RATE_UP                       8192       // kB/s
  #define P2P_DEFAULT_LIMIT_RATE_DOWN                     32768      // kB/s
#endif

#define P2P_FAILED_ADDR_FORGET_SECONDS                  (60*5)      //5 minutes (faster retry for small networks)
#define P2P_IP_BLOCKTIME                                (60*60*24)  //24 hour
#define P2P_IP_FAILS_BEFORE_BLOCK                       10
#define P2P_IDLE_CONNECTION_KILL_INTERVAL               (3*60) //3 minutes

#define P2P_SUPPORT_FLAG_FLUFFY_BLOCKS                  0x01
#define P2P_SUPPORT_FLAGS                               P2P_SUPPORT_FLAG_FLUFFY_BLOCKS

#define RPC_IP_FAILS_BEFORE_BLOCK                       3

#define CRYPTONOTE_NAME                         "ninacatcoin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "data.mdb"
#define CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME "lock.mdb"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define RPC_PAYMENTS_DATA_FILENAME              "rpcpayments.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

#define HF_VERSION_DYNAMIC_FEE                  1
#define HF_VERSION_MIN_MIXIN_4                  1
#define HF_VERSION_MIN_MIXIN_6                  1
#define HF_VERSION_MIN_MIXIN_10                 1
// NINACOIN: HF-based mixin thresholds (legacy — kept as fallback)
#define HF_VERSION_MIN_MIXIN_15                 1
#define HF_VERSION_MIN_MIXIN_20                 255  // Not HF-gated — controlled by Adaptive Ring

// ─── NINA Adaptive Ring System ──────────────────────────────────────────
// Automatic ring size transitions based on RCT output maturity.
// The number of RCT outputs on-chain is monotonically increasing and
// deterministic across all nodes, making this fully consensus-safe.
//
//   Ring 11 (mixin 10)  →  active now (base)
//   Ring 16 (mixin 15)  →  auto-activates at ≥ 100,000 RCT outputs
//   Ring 21 (mixin 20)  →  auto-activates at ≥ 500,000 RCT outputs
//
// Grace period: during the first 5,000 outputs after a threshold,
// both the old and new ring sizes are accepted.
#define NINA_ADAPTIVE_RING_START_HEIGHT   0ULL       // Active from genesis (v18 from block 0)
#define NINA_RING_16_RCT_THRESHOLD        100000ULL  // Ring 11 → 16
#define NINA_RING_21_RCT_THRESHOLD        500000ULL  // Ring 16 → 21
#define NINA_RING_GRACE_OUTPUTS           5000ULL    // Transition window

// ─── NINA v18 Active Ring Control (active from height 3) ───────────────
// NINA transitions from passive observer to active ring controller.
// Threat-level based dynamic ring sizing + intelligent decoy selection.
#define NINA_V18_RING_ACTIVE_HEIGHT       0ULL       // Active from genesis (v18 from block 0)
#define NINA_RING_THREAT_NORMAL_MIXIN     15         // Level 0: Ring 16
#define NINA_RING_THREAT_ELEVATED_MIXIN   20         // Level 1: Ring 21
#define NINA_RING_THREAT_HIGH_MIXIN       25         // Level 2: Ring 26
#define NINA_RING_THREAT_CRITICAL_MIXIN   30         // Level 3: Ring 31
#define NINA_RING_THREAT_HYSTERESIS_UP    10         // Blocks to sustain before raising
#define NINA_RING_THREAT_HYSTERESIS_DOWN  100        // Blocks of normal before lowering
#define NINA_RING_GRACE_BLOCKS            50         // Grace window on ring increase
#define NINA_RING_QUALITY_MIN             0.85f      // Minimum Ring Quality Score
#define NINA_RING_ASM_UPDATE_INTERVAL     720        // Blocks between ASM model updates (~1 day)
#define NINA_RING_POISON_THRESHOLD        0.7f       // Poison score to exclude output
#define NINA_RING_THREAT_ANALYSIS_WINDOW  100        // Blocks for sliding threat window

#define HF_VERSION_ENFORCE_RCT                  1
#define HF_VERSION_PER_BYTE_FEE                 1
#define HF_VERSION_SMALLER_BP                   1
#define HF_VERSION_LONG_TERM_BLOCK_WEIGHT       1
#define HF_VERSION_MIN_2_OUTPUTS                1
#define HF_VERSION_MIN_V2_COINBASE_TX           18  // v2 coinbase required from v18 (height 0); all blocks use v2 coinbase
#define HF_VERSION_SAME_MIXIN                   1
#define HF_VERSION_REJECT_SIGS_IN_COINBASE      1
#define HF_VERSION_ENFORCE_MIN_AGE              1
#define HF_VERSION_EFFECTIVE_SHORT_TERM_MEDIAN_IN_PENALTY 1
#define HF_VERSION_EXACT_COINBASE               1
#define HF_VERSION_CLSAG                        1
#define HF_VERSION_DETERMINISTIC_UNLOCK_TIME    1
#define HF_VERSION_BULLETPROOF_PLUS             1
#define HF_VERSION_VIEW_TAGS                    1
#define HF_VERSION_2021_SCALING                 1
#define HF_VERSION_SHORT_UNLOCK_WINDOW          18  // Short coinbase unlock (60→30) activates at v18 (height 0)

#define PER_KB_FEE_QUANTIZATION_DECIMALS        8
#define CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES 2

#define HASH_OF_HASHES_STEP                     512

#define DEFAULT_TXPOOL_MAX_WEIGHT               648000000ull // 3 days at 300000, in bytes

#define BULLETPROOF_MAX_OUTPUTS                 16
#define BULLETPROOF_PLUS_MAX_OUTPUTS            16

#define CRYPTONOTE_PRUNING_STRIPE_SIZE          4096 // the smaller, the smoother the increase
#define CRYPTONOTE_PRUNING_LOG_STRIPES          3 // the higher, the more space saved
#define CRYPTONOTE_PRUNING_TIP_BLOCKS           5500 // the smaller, the more space saved

#define RPC_CREDITS_PER_HASH_SCALE ((float)(1<<24))

#define DNS_BLOCKLIST_LIFETIME (86400 * 8)

//The limit is enough for the mandatory transaction content with 16 outputs (547 bytes),
//a custom tag (1 byte) and up to 32 bytes of custom data for each recipient.
// (1+32) + (1+1+16*32) + (1+16*32) = 1060
#define MAX_TX_EXTRA_SIZE                       1060

// New constants are intended to go here
namespace config
{
  // Align the default fallback fee with the fixed 0.01 NINA per kB schedule.
  uint64_t const DEFAULT_FEE_ATOMIC_NIA_PER_KB = 500; // Just a placeholder!  Change me!
  uint8_t const FEE_CALCULATION_MAX_RETRIES = 10;
  uint64_t const DEFAULT_DUST_THRESHOLD = ((uint64_t)1000000000); // 0.001 NINA (calibrated for NINA economy)
  uint64_t const BASE_REWARD_CLAMP_THRESHOLD = ((uint64_t)100000000); // pow(10, 8)

  uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 118;
  uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 119;
  uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 142;
  uint16_t const P2P_DEFAULT_PORT = 19020;
  uint16_t const RPC_DEFAULT_PORT = 19021;
  uint16_t const ZMQ_RPC_DEFAULT_PORT = 19022;
  boost::uuids::uuid const NETWORK_ID = { {
      0x52, 0x70, 0xB5, 0x75, 0x65, 0x44, 0x45, 0x65, 0x57, 0x71, 0x40, 0xC2, 0x56, 0xE5, 0xE5, 0x50
    } }; // Bender's nightmare
  std::string const GENESIS_TX = "021e01ff0001808090bbbad6adf00d0386052852b91e90639dd8b798ca1b902e2ed954d1b11839f9ee276f6618c1d7681492020128308d882caee41a6e9cff7c7481938329032f9e7db6bc319129e1998abca60302ee014e696e61636174636f696e2032352f30352f3230313420746f2031362f30322f3230313920474f4f4442594520204e6f20707564652070726f7465676572746520656e746f6e6365732c207065726f2061686f726120766f79206120637265617220616c676f207175652073652064656669656e646120736f6c6f207920736561206a7573746f2e204920636f756c646e27742070726f7465637420796f75206261636b207468656e2c20627574206e6f77204920616d206372656174696e6720736f6d657468696e6720746861742063616e20646566656e6420697473656c6620616e6420626520666169722e00";
  uint32_t const GENESIS_NONCE = 0;

  // Hash domain separators
  const char HASH_KEY_BULLETPROOF_EXPONENT[] = "nia_bp_exp";
  const char HASH_KEY_BULLETPROOF_PLUS_EXPONENT[] = "nia_bp_plus_exp";
  const char HASH_KEY_BULLETPROOF_PLUS_TRANSCRIPT[] = "nia_bp_plus_tx";
  const char HASH_KEY_RINGDB[] = "nia_ringdb";
  const char HASH_KEY_SUBADDRESS[] = "nia_subaddr";
  const unsigned char HASH_KEY_ENCRYPTED_PAYMENT_ID = 0x8d;
  const unsigned char HASH_KEY_WALLET = 0x8c;
  const unsigned char HASH_KEY_WALLET_CACHE = 0x8d;
  const unsigned char HASH_KEY_BACKGROUND_CACHE = 0x8e;
  const unsigned char HASH_KEY_BACKGROUND_KEYS_FILE = 0x8f;
  const unsigned char HASH_KEY_RPC_PAYMENT_NONCE = 0x58;
  const unsigned char HASH_KEY_MEMORY = 'k';
  const unsigned char HASH_KEY_MULTISIG[] = {'M', 'u', 'l', 't' , 'i', 's', 'i', 'g', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  const unsigned char HASH_KEY_MULTISIG_KEY_AGGREGATION[] = "nia_ms_key_agg";
  const unsigned char HASH_KEY_CLSAG_ROUND_MULTISIG[] = "nia_clsag_ms_merge";
  const unsigned char HASH_KEY_TXPROOF_V2[] = "nia_txproof_v2";
  const unsigned char HASH_KEY_CLSAG_ROUND[] = "nia_clsag_round";
  const unsigned char HASH_KEY_CLSAG_AGG_0[] = "nia_clsag_agg_0";
  const unsigned char HASH_KEY_CLSAG_AGG_1[] = "nia_clsag_agg_1";
  const char HASH_KEY_MESSAGE_SIGNING[] = "ninacatcoinMessageSignature";
  const unsigned char HASH_KEY_MM_SLOT = 'm';
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS_SEED[] = "nia_ms_tx_pk_seed";
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS[] = "nia_ms_tx_pk";
  const constexpr char HASH_KEY_TXHASH_AND_MIXRING[] = "nia_txhash_mr";

  // Multisig
  const uint32_t MULTISIG_MAX_SIGNERS{16};

  namespace testnet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 153;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 154;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 163;
    uint16_t const P2P_DEFAULT_PORT = 29020;
    uint16_t const RPC_DEFAULT_PORT = 29021;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 29022;
    boost::uuids::uuid const NETWORK_ID = { {
        0x52, 0x70, 0xB5, 0x75, 0x65, 0x44, 0x45, 0x65, 0x57, 0x71, 0x40, 0xC2, 0x56, 0xE5, 0xE5, 0x51

      } }; // Bender's daydream
    std::string const GENESIS_TX = "021e01ff0001808090bbbad6adf00d03bc9f90a113b9f8378b379723f719aa5af14a6591bd7e3ccff49afcde59fb5b8e6b9202014f7a30d69e367dd4cf6ad316b6baf15783a7ec3149f8aebbf18fdcadf5682c6702ee014e696e61636174636f696e2032352f30352f3230313420746f2031362f30322f3230313920474f4f4442594520204e6f20707564652070726f7465676572746520656e746f6e6365732c207065726f2061686f726120766f79206120637265617220616c676f207175652073652064656669656e646120736f6c6f207920736561206a7573746f2e204920636f756c646e27742070726f7465637420796f75206261636b207468656e2c20627574206e6f77204920616d206372656174696e6720736f6d657468696e6720746861742063616e20646566656e6420697473656c6620616e6420626520666169722e00";
    uint32_t const GENESIS_NONCE = 0;
  }

  namespace stagenet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 124;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 125;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 136;
    uint16_t const P2P_DEFAULT_PORT = 39020;
    uint16_t const RPC_DEFAULT_PORT = 39021;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 39022;
    boost::uuids::uuid const NETWORK_ID = { {
        0x52, 0x70, 0xB5, 0x75, 0x65, 0x44, 0x45, 0x65, 0x57, 0x71, 0x40, 0xC2, 0x56, 0xE5, 0xE5, 0x52
      } }; // Bender's daydream
    std::string const GENESIS_TX = "021e01ff0001808090bbbad6adf00d039f7a3568bcaaa2904825cefb490eadd59718b731b0939d9e26e7ef7611b3ffd8899202016ed954179deb1d6413dad0dff437b10eb7b65c0c8144b02e58c828d55743f67d02ee014e696e61636174636f696e2032352f30352f3230313420746f2031362f30322f3230313920474f4f4442594520204e6f20707564652070726f7465676572746520656e746f6e6365732c207065726f2061686f726120766f79206120637265617220616c676f207175652073652064656669656e646120736f6c6f207920736561206a7573746f2e204920636f756c646e27742070726f7465637420796f75206261636b207468656e2c20627574206e6f77204920616d206372656174696e6720736f6d657468696e6720746861742063616e20646566656e6420697473656c6620616e6420626520666169722e00";
    uint32_t const GENESIS_NONCE = 0;
  }
}

namespace cryptonote
{
  enum network_type : uint8_t
  {
    MAINNET = 0,
    TESTNET,
    STAGENET,
    FAKECHAIN,
    UNDEFINED = 255
  };
  struct config_t
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
    uint16_t const P2P_DEFAULT_PORT;
    uint16_t const RPC_DEFAULT_PORT;
    uint16_t const ZMQ_RPC_DEFAULT_PORT;
    boost::uuids::uuid const NETWORK_ID;
    std::string const GENESIS_TX;
    uint32_t const GENESIS_NONCE;
  };
  inline const config_t& get_config(network_type nettype)
  {
    static const config_t mainnet = {
      ::config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::P2P_DEFAULT_PORT,
      ::config::RPC_DEFAULT_PORT,
      ::config::ZMQ_RPC_DEFAULT_PORT,
      ::config::NETWORK_ID,
      ::config::GENESIS_TX,
      ::config::GENESIS_NONCE
    };
    static const config_t testnet = {
      ::config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::testnet::P2P_DEFAULT_PORT,
      ::config::testnet::RPC_DEFAULT_PORT,
      ::config::testnet::ZMQ_RPC_DEFAULT_PORT,
      ::config::testnet::NETWORK_ID,
      ::config::testnet::GENESIS_TX,
      ::config::testnet::GENESIS_NONCE
    };
    static const config_t stagenet = {
      ::config::stagenet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::stagenet::P2P_DEFAULT_PORT,
      ::config::stagenet::RPC_DEFAULT_PORT,
      ::config::stagenet::ZMQ_RPC_DEFAULT_PORT,
      ::config::stagenet::NETWORK_ID,
      ::config::stagenet::GENESIS_TX,
      ::config::stagenet::GENESIS_NONCE
    };
    switch (nettype)
    {
      case MAINNET: return mainnet;
      case TESTNET: return testnet;
      case STAGENET: return stagenet;
      case FAKECHAIN: return mainnet;
      default: throw std::runtime_error("Invalid network type");
    }
  };
}
