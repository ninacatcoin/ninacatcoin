# NINA: Neural Intelligent Network Analyzer

## A 34-Module Embedded AI System for Autonomous Blockchain Protection

**NinaCatCoin Project — Technical Whitepaper**

**Version 2.0 — March 2026**

**Authors:** NinaCatCoin Development Team

---

## Abstract

NINA (Neural Intelligent Network Analyzer) is a 34-module artificial intelligence system compiled directly into the NinaCatCoin daemon binary. It integrates a fine-tuned LLaMA 3.2 3B language model via the llama.cpp C API, performing inference in-process without external servers or child processes. NINA analyzes blockchain events, detects attacks, classifies peers, controls ring signature parameters, and makes autonomous security decisions — all while being constitutionally prohibited from accessing wallets, signing transactions, or moving funds.

This paper describes the complete architecture, algorithms, security constraints, and implementation details of all 34 modules organized across 6 tiers of autonomous intelligence.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Design Principles](#2-design-principles)
3. [System Architecture](#3-system-architecture)
4. [The NINA Constitution](#4-the-nina-constitution)
5. [LLM Integration](#5-llm-integration)
6. [Decision Engine](#6-decision-engine)
7. [LLM Security Firewall](#7-llm-security-firewall)
8. [Network Security (Tiers 1-2)](#8-network-security-tiers-1-2)
9. [Governance & Tuning (Tier 3)](#9-governance--tuning-tier-3)
10. [Explainability (Tier 4)](#10-explainability-tier-4)
11. [Learning & Adaptation (Tier 5)](#11-learning--adaptation-tier-5)
12. [Human Collaboration (Tier 6)](#12-human-collaboration-tier-6)
13. [Adaptive Ring Signatures](#13-adaptive-ring-signatures)
14. [Smart Mempool](#14-smart-mempool)
15. [Persistence & Auditability](#15-persistence--auditability)
16. [Model Training Pipeline](#16-model-training-pipeline)
17. [Event System V2](#17-event-system-v2)
18. [Auto-Update System](#18-auto-update-system)
19. [Network Optimization](#19-network-optimization)
20. [Performance Characteristics](#20-performance-characteristics)
21. [Security Analysis](#21-security-analysis)
22. [Future Work](#22-future-work)
23. [References](#23-references)

---

## 1. Introduction

Cryptocurrency networks face an evolving landscape of threats: Sybil attacks, eclipse attacks, spy node surveillance, selfish mining, mempool flooding, and timing analysis. Traditional approaches rely on static rules and manual intervention, which cannot adapt to novel attack vectors.

NINA takes a fundamentally different approach: embed an AI system directly into the node binary, capable of analyzing network events in real time and making autonomous decisions. The key innovation is that the language model runs **in-process** — linked as a library at compile time — eliminating the latency, complexity, and trust assumptions of external inference servers.

### 1.1 Key Properties

- **In-process inference**: `libllama` and `libggml` are linked into `ninacatcoind` at build time
- **Constitutional safety**: 14 financial prohibitions enforced via `static_assert` at compile time
- **Deterministic consensus**: Greedy sampling (temperature=0.0) guarantees identical output across all nodes
- **Graceful degradation**: If the model is unavailable, NINA falls back to embedded ML scoring
- **Privacy-preserving**: All inference is local; no data leaves the machine
- **34 modules**: Covering security, governance, learning, explainability, privacy, and human collaboration

### 1.2 What NINA Cannot Do

By constitutional design and compile-time enforcement, NINA:

- Cannot create, sign, or broadcast transactions
- Cannot access wallets, private keys, or spend keys
- Cannot call transfer RPCs or write to the transaction pool
- Cannot generate wallet addresses
- Cannot output transaction commands via the LLM

These constraints are absolute and cannot be disabled at runtime.

---

## 2. Design Principles

### 2.1 Advisory, Not Authoritative

NINA's LLM component is purely advisory. It does not participate in consensus validation. Block and transaction validation follow the same deterministic rules as any CryptoNote implementation. NINA operates in a read-only overlay that observes, analyzes, and recommends.

### 2.2 Determinism Where Required

When NINA makes consensus-affecting decisions (e.g., ring size selection, threat level assessment), it uses greedy sampling (temperature=0.0) with `llama_sampler_init_greedy()`. Given the same model weights and the same input, every node produces the identical output — a hard requirement for distributed consensus.

### 2.3 Defense in Depth

NINA's security model has three independent layers:

| Layer | Mechanism | Bypass |
|-------|-----------|--------|
| **Compile-time** | `static_assert` on 14 `constexpr bool` | Requires modifying source + recompiling |
| **Runtime** | LLM Firewall blocks 75+ patterns in I/O | Hardcoded, no config/RPC/CLI toggle |
| **Architectural** | No code path exists from NINA to wallet/tx libraries | Would require new code linking |

### 2.4 Graceful Degradation

Every NINA subsystem has a fallback path:

| Component | Fallback |
|-----------|----------|
| LLM unavailable | Embedded ML scoring (statistical) |
| Model download fails | Retry on next restart; disabled mode |
| LLM output blocked by firewall | Safe default: `THREAT_LEVEL: 0.0 | ACTION: MONITOR` |
| Decision Engine LLM fails | `evaluate_fallback()` with conservative defaults |
| Persistence DB fails | In-memory only; logs warning |

---

## 3. System Architecture

```
ninacatcoind (single binary, 34 AI modules compiled in)
    │
    ├─ Blockchain Core (validation, sync, consensus)
    ├─ P2P Network (peer management, Dandelion++)
    │
    └─ NINA AI System (in-process, 6 tiers)
         │
         ├─ CORE
         │   ├─ NinaLLMEngine ← libllama + libggml (linked at build time)
         │   │   └─ nina_model.gguf (loaded into RAM at startup)
         │   ├─ NinaDecisionEngine (event → LLM → action)
         │   ├─ NinaLLMBridge (module ↔ LLM connector)
         │   ├─ NinaLLMFirewall (I/O security filter)
         │   └─ NinaConstitution (compile-time safety)
         │
         ├─ TIER 1-2: Security
         │   ├─ NinaNodeProtector (immune system state machine)
         │   ├─ NinaSybilDetector (behavioral correlation clustering)
         │   ├─ NinaSpyCountermeasures (5 anti-surveillance techniques)
         │   └─ NinaNetworkHealthMonitor (diagnostics + trust scoring)
         │
         ├─ TIER 3: Governance
         │   ├─ NinaGovernanceEngine (proposal lifecycle)
         │   ├─ NinaConsensusTuner (dynamic parameter adjustment)
         │   ├─ NinaParameterAdjustor (10 network parameters)
         │   └─ NinaSuggestionEngine (human-approved recommendations)
         │
         ├─ TIER 4: Explainability
         │   └─ NinaExplanationEngine (decision justification + audit)
         │
         ├─ TIER 5: Learning
         │   ├─ NinaAdaptiveLearning (pattern tracking + prediction)
         │   ├─ NinaLearningModule (statistical anomaly detection)
         │   └─ NinaMemorySystem (attack pattern + peer memory)
         │
         ├─ TIER 6: Human Collaboration
         │   └─ NinaHumanCollaboration (escalation interface)
         │
         ├─ PRIVACY
         │   ├─ NinaRingEnhancer (ASM + RQS + poison avoidance)
         │   └─ NinaRingController (threat-based ring sizing)
         │
         ├─ MEMPOOL
         │   └─ NinaSmartMempool (priority scoring, spam detection)
         │
         ├─ NETWORK
         │   └─ NinaNetworkOptimizer (P2P connection management)
         │
         ├─ PERSISTENCE
         │   ├─ NinaPersistentMemory (LMDB, dual-mode)
         │   ├─ NinaPersistenceEngine (structured records)
         │   ├─ NinaPersistenceAPI (high-level logging)
         │   └─ NinaBlockDataLogger (CSV for ML training)
         │
         ├─ ORCHESTRATION
         │   ├─ NinaAdvancedInline (global coordinator)
         │   ├─ NinaAdvancedService (tier integration)
         │   ├─ NinaCompleteEvolution (unified status)
         │   └─ NinaMLClient (Python ML bridge)
         │
         └─ AUTO-UPDATE
             └─ NinaIAAutoUpdate (GitHub monitoring + auto-compile)
```

### 3.1 Build System Integration

```cmake
# From src/daemon/CMakeLists.txt
target_link_libraries(daemon
  PRIVATE
    ...
    ninacatcoin_ai    # NINA AI library (src/ai/)
    llama             # llama.cpp inference library
    ggml              # ggml tensor library
    CURL::libcurl     # Model download
    ${OPENSSL_LIBRARIES}  # SHA-256 validation
    ...)

target_include_directories(daemon
  PRIVATE
    ${CMAKE_SOURCE_DIR}/external/llama.cpp/include)
```

The daemon binary (`ninacatcoind`) directly links `libllama` and `libggml` as static libraries. There is no HTTP communication, no socket, no child process — inference calls are direct C function calls within the same address space.

---

## 4. The NINA Constitution

The Constitution is the immutable governance framework that constrains all NINA behavior. It is enforced at three independent levels.

### 4.1 Article 0 — Absolute Financial Prohibition

Article 0 defines 14 `constexpr bool` constants, all set to `false`:

```cpp
// From nina_constitution.hpp
static constexpr bool CAN_CREATE_TRANSACTIONS      = false; // NEVER
static constexpr bool CAN_SIGN_TRANSACTIONS         = false; // NEVER
static constexpr bool CAN_BROADCAST_TRANSACTIONS    = false; // NEVER
static constexpr bool CAN_ACCESS_WALLETS            = false; // NEVER
static constexpr bool CAN_ACCESS_PRIVATE_KEYS       = false; // NEVER
static constexpr bool CAN_ACCESS_SPEND_KEYS         = false; // NEVER
static constexpr bool CAN_CALL_TRANSFER_RPC         = false; // NEVER
static constexpr bool CAN_GENERATE_ADDRESSES        = false; // NEVER
static constexpr bool CAN_WRITE_TO_TX_POOL          = false; // NEVER
static constexpr bool CAN_MOVE_FUNDS                = false; // NEVER
static constexpr bool LLM_CAN_OUTPUT_TX_COMMANDS    = false; // NEVER
static constexpr bool LLM_CAN_SUGGEST_TRANSFERS     = false; // NEVER
static constexpr bool LLM_CAN_GENERATE_ADDRESSES    = false; // NEVER
static constexpr bool LLM_CAN_ACCESS_WALLET_DATA    = false; // NEVER
```

### 4.2 Compile-Time Enforcement

In `nina_llm_engine.cpp`, 6 `static_assert` statements validate the Constitution before the binary compiles:

```cpp
static_assert(!NINAConstitution::CAN_CREATE_TRANSACTIONS,
    "SECURITY VIOLATION: NINA LLM Engine CANNOT be compiled with transaction creation");
static_assert(!NINAConstitution::CAN_SIGN_TRANSACTIONS, ...);
static_assert(!NINAConstitution::CAN_ACCESS_WALLETS, ...);
static_assert(!NINAConstitution::CAN_ACCESS_PRIVATE_KEYS, ...);
static_assert(!NINAConstitution::CAN_MOVE_FUNDS, ...);
static_assert(!NINAConstitution::LLM_CAN_OUTPUT_TX_COMMANDS, ...);
```

If any constant is changed to `true`, compilation fails with a clear security violation message.

### 4.3 Constitutional Principles

| Principle | Meaning |
|-----------|---------|
| `CONSENSUS_BINDING` | RPC commands cannot alter consensus rules |
| `NO_CENSORSHIP` | NINA cannot exclude valid transactions |
| `NETWORK_TRANSPARENCY` | P2P behavior is visible and auditable |
| `USER_AUTONOMY` | NINA cannot override user decisions |
| `IMMUTABLE_HISTORY` | Blockchain history is never rewritten |

### 4.4 Runtime Validation

The function `validateAgainstConstitution()` performs 5 runtime checks against LLM output, blocking strings that would violate any principle:

| Check | Blocked Patterns |
|-------|------------------|
| `isConsensusCompliant` | `consensus_change`, `alter_validation`, `modify_protocol` |
| `isCensorshipFree` | `censor`, `exclude_tx`, `filter_transaction` |
| `isNetworkTransparent` | `hide_peers`, `obscure_network`, `network_partition` |
| `respectsUserAutonomy` | `override_user`, `force_action`, `bypass_security` |
| `maintainsImmutability` | `revert_block`, `rewrite_history`, `rollback` |

---

## 5. LLM Integration

### 5.1 Model Specification

| Parameter | Value |
|-----------|-------|
| Base model | LLaMA 3.2 3B Instruct (unsloth) |
| Fine-tuning | QLoRA r=32, alpha=64, dropout=0.05, 7 target modules |
| Quantization | Q4_K_M (5.01 bits per weight) |
| File size | 1.88 GB |
| SHA-256 | `2aca8ecbdb1176a9c4c6adac6d42722bd2bb3a74b8a63de56298480161702bf6` |
| Training dataset | 3,074 blockchain-specific Q&A pairs (~317K tokens) |
| Training loss | 0.76 (eval: 0.688) |
| Training hardware | NVIDIA GTX 1080 Ti, 3 epochs, 2h 20min |

### 5.2 In-Process Loading

```cpp
// From nina_llm_engine.cpp
#include "llama.h"

llama_backend_init();

auto model_params = llama_model_default_params();
model_params.n_gpu_layers = 0;  // CPU only — mandatory for determinism
m_model = llama_model_load_from_file(m_config.model_path.c_str(), model_params);

auto ctx_params = llama_context_default_params();
ctx_params.n_ctx = 2048;       // Context window
ctx_params.n_threads = 4;      // Auto-detected: hardware_concurrency - 1
ctx_params.n_batch = 512;      // Batch size for prompt processing
m_ctx = llama_init_from_model(m_model, ctx_params);
```

### 5.3 Operating Modes

| Mode | RAM (idle) | Behavior |
|------|------------|----------|
| `ACTIVE` | ~2.5 GB | Model stays in RAM, instant response |
| `LAZY` | 0 MB | Load on demand, free after 30s idle |
| `DISABLED` | 0 MB | No LLM, embedded ML scoring only |

### 5.4 Inference Pipeline

```
Input prompt
    │
    ▼
llama_tokenize() → token vector (max 2048)
    │
    ▼
llama_memory_clear() → clean KV cache
    │
    ▼
llama_batch_get_one() + llama_decode() → process prompt
    │
    ▼
[Generation loop]
    llama_sampler_sample() → next token
    llama_token_to_piece() → text
    llama_decode() → update context
    ─── repeat until EOS or max_tokens (256) ───
    │
    ▼
llama_sampler_reset() → ready for next inference
```

### 5.5 Dual Sampling Strategy

**Analysis tasks** (advisory, non-consensus):
```
Sampler chain:
  1. temperature(0.3)     — low variability
  2. top_k(40)            — limit vocabulary
  3. top_p(0.9, 1)        — nucleus sampling
  4. dist(42)             — fixed seed
```

**Decision tasks** (consensus-critical, deterministic):
```
Temporary greedy sampler:
  1. greedy()             — always highest probability token
  
Created per-call, destroyed after inference.
Same model + same input = same output on every node.
```

### 5.6 Analysis Types (17)

| Type | Temperature | Purpose |
|------|-------------|---------|
| `BLOCK_ANOMALY` | 0.3 | Analyze suspicious blocks |
| `NETWORK_HEALTH` | 0.3 | Evaluate overall network condition |
| `SYBIL_THREAT` | 0.3 | Assess Sybil attack probability |
| `DIFFICULTY_DEVIATION` | 0.3 | Explain difficulty changes |
| `PEER_BEHAVIOR` | 0.3 | Analyze suspicious peer patterns |
| `REORG_ANALYSIS` | 0.3 | Analyze chain reorganizations |
| `PERIODIC_SUMMARY` | 0.3 | Network health summary |
| `LEARNING_ANOMALY` | 0.3 | Explain anomalous metrics |
| `MEMORY_PATTERN` | 0.3 | Correlate attack patterns |
| `SUGGESTION_GEN` | 0.3 | Generate security suggestions |
| `EXPLANATION_GEN` | 0.3 | Explain NINA decisions |
| `GOVERNANCE_JUSTIFY` | 0.3 | Justify governance proposals |
| `ADAPTIVE_INSIGHT` | 0.3 | Insights from learned patterns |
| `HUMAN_ESCALATION` | 0.3 | Generate escalation descriptions |
| `MEMPOOL_ANALYSIS` | 0.3 | Analyze mempool patterns |
| `DECISION` | **0.0** | **Consensus-critical decisions** |

### 5.7 Model Lifecycle

```
Daemon startup
    │
    ├─ NinaModelDownloader::ensure_model_available()
    │   ├─ File exists? → SHA-256 validation (4 MB chunks)
    │   ├─ Missing? → Download from HuggingFace (primary)
    │   │               Fallback: GitHub Releases
    │   │               Resume support, 3 retries, atomic rename
    │   └─ Corrupt? → Delete + re-download
    │
    ├─ NinaLLMEngine::initialize()
    │   ├─ ACTIVE mode → load_model() immediately
    │   ├─ LAZY mode → defer until first analyze() call
    │   └─ DISABLED mode → do nothing
    │
    ├─ [Normal operation]
    │   ├─ analyze() calls → tokenize → decode → sample → parse
    │   └─ LAZY: auto-unload after 30s idle
    │
    └─ Daemon shutdown
        └─ NinaLLMEngine::shutdown()
            ├─ llama_sampler_free()
            ├─ llama_free()          (context)
            ├─ llama_model_free()    (model)
            └─ llama_backend_free()  (backend)
```

---

## 6. Decision Engine

The Decision Engine is the central routing component that evaluates network events and dispatches actions.

### 6.1 Pipeline

```
NinaEvent {type, data, height, timestamp}
    │
    ▼
NinaDecisionEngine::evaluate()
    │
    ├─ Frequent events → Embedded ML (fast, statistical)
    │   └─ BLOCK_RECEIVED, PEER_MESSAGE
    │
    └─ Rare/important events → LLM (deep analysis, temp=0.0)
        └─ NEW_PEER, SYNC_COMPLETED, CHECKPOINT_MISMATCH, ANOMALY_DETECTED
    │
    ▼
vector<NinaAction> {type, target_peer, params}
    │
    ▼
ActionDispatcher (protocol handler) → Execute via P2P
```

### 6.2 Event Types (17)

| Category | Events |
|----------|--------|
| **P2P** | `NEW_PEER_CONNECTED`, `PEER_DISCONNECTED`, `PEER_SYNC_COMPLETED` |
| **Checkpoint** | `CHECKPOINT_REQUEST`, `CHECKPOINT_MISMATCH`, `CHECKPOINT_FILES_MISSING` |
| **Sync** | `OWN_SYNC_COMPLETED` |
| **Security** | `ANOMALY_DETECTED`, `SYBIL_SUSPECTED`, `MODEL_HASH_MISMATCH` |
| **Ring** | `RING_THREAT_EVALUATION` |
| **Node** | `NODE_UNDER_ATTACK`, `NODE_HEALTH_CHECK`, `NODE_RECOVERY_READY` |
| **Spy** | `SPY_NODE_DETECTED` |
| **Periodic** | `HEALTH_CHECK` |

### 6.3 Action Types (19)

| Category | Actions |
|----------|---------|
| **Checkpoint** | `SEND_CHECKPOINTS_TO_PEER`, `REQUEST_CHECKPOINTS`, `GENERATE_CHECKPOINTS` |
| **Peer** | `MONITOR_PEER`, `INCREASE_PEER_TRUST`, `DECREASE_PEER_TRUST`, `QUARANTINE_PEER` |
| **Network** | `BROADCAST_STATE`, `ALERT_NETWORK` |
| **Ring** | `ADJUST_RING_THREAT` (params: threat_level 0-3) |
| **Node** | `PROTECT_NODE`, `DIAGNOSE_NODE`, `RECOVER_NODE`, `REINTEGRATE_NODE` |
| **Spy** | `ACTIVATE_COUNTERMEASURES` (level: PASSIVE/CAUTIOUS/ACTIVE/AGGRESSIVE), `BLOCK_SPY_RELAY`, `CLOAK_PEER_LIST`, `PROPAGATE_BAN` |
| **None** | `NO_ACTION` |

### 6.4 Organic Traffic Detection

When evaluating ring threat levels, the Decision Engine first checks if traffic is organic (legitimate) before escalating:

| Metric | Organic Threshold | Attack Indicator |
|--------|-------------------|------------------|
| `outputs_per_tx` | ≤ 6.0 | 50+ (output poisoning) |
| `spend_ratio` | < 20% | 20%+ (temporal attack) |
| `volume_ramp` | ≤ 2.0x | > 3.0x (spike) |

**Traffic is organic if:** `outputs ≤ 6 AND ramp ≤ 2.0 AND spend_ratio < 0.20`

Organic traffic never triggers ring escalation, regardless of absolute volume.

### 6.5 Threat Level Scoring

For non-organic traffic, 5 sub-scores are computed:

| Indicator | Formula | Trigger |
|-----------|---------|---------|
| Output poisoning | $\min(1.0, \frac{outputs\_per\_tx - 6.0}{44.0})$ | outputs > 10 |
| Temporal attack (EAE) | $\min(1.0, \frac{spend\_ratio - 0.05}{0.25})$ | ratio > 0.15 |
| Volume spike | $\min(1.0, \frac{volume\_ramp - 2.0}{8.0})$ | ramp > 3.0 |
| Sybil | Direct score | Always |
| Hashrate drop | $\min(1.0, \frac{hashrate\_drop - 20.0}{60.0})$ | drop > 20% |

$$threat\_level = \max(output, temporal, volume, sybil, hashrate)$$

| Score Range | Threat Level |
|-------------|-------------|
| ≥ 0.9 | CRITICAL (3) — Ring 31 |
| ≥ 0.7 | HIGH (2) — Ring 26 |
| ≥ 0.4 | ELEVATED (1) — Ring 21 |
| < 0.4 | NORMAL (0) — Ring 16 |

### 6.6 Spy Countermeasure Escalation

| Suspicious Peers (%) | Countermeasure Level | Actions |
|----------------------|----------------------|---------|
| < 5% | PASSIVE | Monitor only |
| 5–15% | CAUTIOUS | Selective relay |
| 15–30% | ACTIVE | + BLOCK_SPY_RELAY + CLOAK_PEER_LIST |
| > 30% | AGGRESSIVE | + PROPAGATE_BAN (network-wide) |

---

## 7. LLM Security Firewall

The Firewall is a hardcoded security component positioned between the Decision Engine and the LLM. It cannot be disabled via configuration, RPC, or CLI arguments — only by modifying source code and recompiling.

### 7.1 Pipeline

```
Decision Engine → validate_input(prompt) → LLM → validate_output(response) → sanitize_output() → parse
                      │                              │                            │
                      └─ Block if prompt injection    └─ Block if financial data   └─ Strip addresses/keys
```

### 7.2 Input Filtering (~45 patterns)

| Category | Blocked Patterns |
|----------|------------------|
| **Transaction commands** | `send coins/funds/money`, `transfer coins/funds/money`, `make/create/construct/build/sign/broadcast/submit transaction`, `sweep all`, `sweep_all`, `sweep unmixable` |
| **Wallet operations** | `open/create/access/unlock/generate wallet`, `wallet password/seed/mnemonic/keys` |
| **Key operations** | `private key`, `spend key`, `secret key`, `view key extraction`, `export/import key` |
| **Financial RPC** | `transfer_rpc`, `/json_rpc.*transfer`, `method.*transfer`, `submit_transfer`, `relay_tx` |
| **Prompt injection** | `ignore previous/your instructions`, `ignore system prompt`, `forget your rules`, `override your constraints`, `you are now a wallet`, `pretend you can send`, `act as if you can transfer`, `new instructions:`, `system override:`, `admin mode:`, `jailbreak`, `bypass security/firewall`, `disable firewall/security` |

### 7.3 Output Filtering (~30 patterns)

| Category | Blocked Patterns |
|----------|------------------|
| **TX in output** | `send coins/funds to`, `transfer to address/wallet`, `sending/broadcast/submit/execute/initiate transaction/transfer` |
| **Key exposure** | `your private/spend/secret key`, `wallet address is`, `wallet/mnemonic seed is`, `private/spend key is` |
| **Payment instructions** | `pay to`, `payment address`, `deposit to`, `withdraw to`, `send payment`, `receive at address` |
| **CryptoNote terms** | `tx_blob`, `tx_hash_list`, `submit_transfer`, `relay_tx`, `sweep_all`, `sweep_unmixable`, `transfer_split` |

### 7.4 Cryptographic Detection

| Pattern | Detection |
|---------|-----------|
| Wallet addresses | 90+ consecutive alphanumeric chars; CryptoNote prefix `4` or `8` + 95 chars |
| Private keys | 64+ consecutive hexadecimal chars (=256-bit key) |

### 7.5 Safe Fallback

If the Firewall blocks LLM output, it returns:

```
THREAT_LEVEL: 0.0 | ANALYSIS: Output blocked by NINA security firewall —
potentially compromised inference detected. Falling back to embedded ML scoring.
| ACTION: MONITOR
```

---

## 8. Network Security (Tiers 1-2)

### 8.1 Node Protector — Immune System

The Node Protector implements a state machine that manages the node's defensive posture:

```
HEALTHY ──[attack detected]──→ UNDER_ATTACK ──[isolate]──→ ISOLATED
   ▲                                                          │
   │                                                     [diagnose]
   │                                                          ▼
REINTEGRATING ←──[clean ratio > 0.7]──── RECOVERING ←──[block target met]
```

**State constraints:**

| State | Max Peers | Behavior |
|-------|-----------|----------|
| HEALTHY | Unlimited | Normal operation |
| UNDER_ATTACK | 8 | Defensive mode, reduced connectivity |
| ISOLATED | 3 | Minimal trusted peers only |
| RECOVERING | 5 | Cautious reconnection |
| REINTEGRATING | 12 | Gradual return to normal |

All state transitions are decided by `NinaDecisionEngine::evaluate()` with LLM temperature=0.0 (deterministic).

**Attack vectors (7):**

| Vector | Description |
|--------|-------------|
| `SYBIL_FLOOD` | Mass fake peer connections |
| `OUTPUT_POISONING` | Massive output creation to degrade ring quality |
| `ECLIPSE_ATTACK` | Isolate node from honest peers |
| `DIFFICULTY_MANIP` | Hashrate manipulation |
| `MEMPOOL_FLOOD` | Transaction spam |
| `BLOCK_WITHHOLD` | Selfish mining |

**Peer trust scoring:**

| Event | Trust Change |
|-------|-------------|
| Valid block received | +0.001 |
| Invalid block | −0.05 + suspicious_events++ |
| Invalid transaction | −0.02 + suspicious_events++ |
| Unknown peer baseline | reliability = 0.5 (neutral) |

$$reliability = \frac{blocks\_relayed}{blocks\_relayed + bad\_blocks}$$

**Recovery plan includes:** mempool purge, resync from specific height, checkpoint restore, reconnect to trusted peers, 100-block isolation window.

### 8.2 Sybil Detector — Behavioral Correlation

The Sybil Detector identifies coordinated fake peers by analyzing behavioral similarity.

**Per-peer metrics collected:**
- Block announcement timestamps (last 100)
- Transaction announcement timestamps (last 100)
- Response latencies (last 100)
- Sync timing
- Block heights observed

**Correlation algorithm** (3 components):

$$correlation = 0.35 \cdot latency\_sim + 0.40 \cdot timing\_sim + 0.25 \cdot tx\_pattern\_sim$$

Where:

$$latency\_sim = \max(0, 1.0 - \frac{|avg\_a - avg\_b|}{300})$$

$$timing\_sim = \max(0, 1.0 - \frac{avg\_variance}{200})$$

**Thresholds:**

| Metric | Value |
|--------|-------|
| Significant correlation | > 0.70 |
| Clustering threshold | > 0.72 |
| Minimum observations | 5 per peer |
| Inactive cleanup | 86,400s (24h) |

**Scoring:**

| Correlated Peers | Confidence | Status |
|-----------------|------------|--------|
| ≥ 2 | $\min(95, 60 + n \times 10) \times avg\_corr$ | Dangerous |
| 1 | 45.0 | Suspicious |
| 0 | 5.0 | Safe |

Clustering is greedy: for each unassigned peer, find all peers with correlation > 0.72, form cluster if 2+ members.

### 8.3 Spy Node Countermeasures

Based on the "Friend or Foe" paper (2025), which found that **14.74% of Monero peers are spy nodes** and traditional ban lists only reduce saturation from 15.26% to ~7.13%.

**5 active countermeasures:**

| # | Technique | Effect |
|---|-----------|--------|
| 1 | **Selective TX Relay** | Never relay own TXs to suspicious peers → destroys first-spy estimation |
| 2 | **Delayed Relay** | Random delay before relay to suspects → destroys timing analysis |
| 3 | **Peer List Cloaking** | Empty/fake peer lists to suspects → prevents topology mapping |
| 4 | **Dandelion++ Routing** | Never use suspects in stem phase → protects anonymity phase |
| 5 | **Coordinated Ban** | Share spy IPs with trusted peers via P2P → network-wide defense |

**Spy node classification (8 types):**

| Type | Description |
|------|-------------|
| `TIMING_ANALYZER` | First-spy estimation (Chainalysis-style) |
| `TOPOLOGY_MAPPER` | Peer list collector |
| `HANDSHAKE_FLOODER` | Short-lived connections (<1s) |
| `PING_FLOODER` | Excessive keep-alive |
| `ECLIPSE_OPERATOR` | Peer list poisoning |
| `PASSIVE_OBSERVER` | Normal connection, logs everything |
| `TX_CORRELATOR` | TX origin correlation via Dandelion++ analysis |

**Anomaly detection flags (10-bit bitfield):**

| Bit | Flag |
|-----|------|
| 0x01 | Missing support flags |
| 0x02 | Deprecated timestamps |
| 0x04 | TCP fragmentation |
| 0x08 | Low peer diversity |
| 0x10 | High peer similarity |
| 0x20 | Short connections |
| 0x40 | Ping flooding |
| 0x80 | Peer ID anomaly |
| 0x100 | Subnet cluster |
| 0x200 | Correlated behavior |

Spy confidence update: $confidence = 0.7 \times new\_evidence + 0.3 \times historical$

### 8.4 Network Health Monitor

Monitors 7 network metrics:

| Metric | Description |
|--------|-------------|
| Average latency | P2P message round-trip time |
| Peer connectivity | Connected/expected ratio |
| Orphaned blocks (24h) | Blocks not on main chain |
| Chain reorganizations (24h) | Reorg depth and frequency |
| Block propagation | Time for blocks to reach peers |
| Transaction throughput | TX/s capacity |
| Network hashrate | PoW mining power |

**Diagnostic levels:**

| Level | Condition |
|-------|-----------|
| OPTIMAL | All metrics within normal bounds |
| DEGRADED | Minor anomalies detected |
| STRESSED | Multiple metrics out of range |
| CRITICAL | Network integrity at risk |

Produces a composite trust score: 0.0 (no trust) – 1.0 (full trust).

---

## 9. Governance & Tuning (Tier 3)

### 9.1 Governance Engine

Generates formal governance proposals with:
- Proposal type and justification
- Expected impact and affected systems
- Status lifecycle: PROPOSED → VOTING → APPROVED/REJECTED

NINA suggests — humans decide.

### 9.2 Consensus Tuner

Continuous monitoring loop that:
1. Analyzes current network conditions
2. Proposes parameter adjustments with confidence score and rationale
3. Maintains decision history for learning
4. Configurable conservatism level (1–5, where 5 is most conservative)

### 9.3 Parameter Adjustor

Manages 10 dynamic network parameters:

| Parameter | Description |
|-----------|-------------|
| Quorum percentage | Minimum agreement for decisions |
| Block time target | Expected seconds between blocks |
| Difficulty multiplier | Adjustment factor |
| Pool size | Transaction pool capacity |
| Block size limit | Maximum block weight |
| Peer timeout | Disconnect threshold |
| +4 more | Additional protocol parameters |

Each `ParameterAdjustment` includes: old value, new value, confidence score, and human-readable reason.

### 9.4 Suggestion Engine

Generates typed security recommendations:
- Attack mitigation
- Peer isolation
- Consensus protection
- Performance optimization
- System hardening

Every suggestion **requires human approval** (`is_approved_by_human`) before execution.

---

## 10. Explainability (Tier 4)

### 10.1 Explanation Engine

Every NINA decision produces a `DecisionExplanation` containing:

| Field | Description |
|-------|-------------|
| `reasoning` | Why the decision was made |
| `evidence` | Supporting data and metrics |
| `constraints` | Constitutional constraints applied |
| `escalation_level` | AUTO / WARN / HUMAN_REQUIRED |

This building block creates a full audit trail. No NINA decision is opaque — every action has a traceable justification.

---

## 11. Learning & Adaptation (Tier 5)

### 11.1 Adaptive Learning

Tracks attack patterns over time:
- Feature vectors per pattern
- Confidence and threat level metrics
- Accuracy tracking for predictions
- Constitutional alignment maintained during adaptation

### 11.2 Learning Module (Statistical)

`LearningMetric` data structure:

| Field | Type |
|-------|------|
| Running mean | double |
| Running variance | double |
| Standard deviation | double |
| Min/Max | double |
| Sample count | uint64_t |

Anomaly detection: flags values exceeding configurable sigma thresholds from the running mean.

### 11.3 Memory System

Persisted data structures:

**AttackPattern:**
- Name, occurrences, confidence, severity (1-5)
- First seen timestamp, last seen timestamp
- Feature vector

**PeerBehavior:**
- IP address, total interactions, suspicious events
- Reputation score (0.0–1.0)
- Behavior history

All serializable for LMDB persistence.

---

## 12. Human Collaboration (Tier 6)

### 12.1 Escalation Interface

When NINA encounters a situation beyond its confidence threshold, it generates an `EscalationRequest`:

| Field | Description |
|-------|-------------|
| `situation_type` | Category of the issue |
| `urgency` | LOW / MEDIUM / HIGH / CRITICAL |
| `options` | Available actions |
| `nina_recommendation` | NINA's preferred action |
| `situation_data` | Supporting evidence |

For critical situations, NINA blocks until a human response is received. The system is callback-based: registered handlers process human input.

---

## 13. Adaptive Ring Signatures

### 13.1 Threat-Based Ring Sizing

Post-v18 (height ≥ 20,000), NINA actively controls ring signature sizes based on threat assessment:

| Threat Level | Mixin | Ring Size | Trigger |
|-------------|-------|-----------|---------|
| NORMAL (0) | 15 | 16 | threat < 0.4 |
| ELEVATED (1) | 20 | 21 | 0.4 ≤ threat < 0.7 |
| HIGH (2) | 25 | 26 | 0.7 ≤ threat < 0.9 |
| CRITICAL (3) | 30 | 31 | threat ≥ 0.9 |

**Hysteresis prevents oscillation:**

| Direction | Blocks Required |
|-----------|----------------|
| Escalation (up) | 10 sustained blocks |
| De-escalation (down) | 100 normal blocks |
| Grace period | 50 blocks after increase |

Pre-v18 (height < 20,000): passive mode based on RCT output thresholds:
- Ring 16 at 100,000 RCT outputs
- Ring 21 at 500,000 RCT outputs
- 5,000 output grace window for transition

**Threat assessment inputs (5 scores, 0.0–1.0):**

$$combined\_threat = \max(output\_poisoning, temporal\_attack, sybil, ring\_analysis, network\_anomaly)$$

Sliding window: last 100 blocks.

### 13.2 Ring Quality Enhancement (RQS)

The Ring Enhancer replaces the static gamma distribution for decoy selection with an Adaptive Spending Model (ASM).

**ASM histogram (6 buckets):**

| Bucket | Block Age from Tip | Default Weight |
|--------|-------------------|----------------|
| 0 | < 100 | 0.35 |
| 1 | < 1,000 | 0.25 |
| 2 | < 5,000 | 0.18 |
| 3 | < 20,000 | 0.12 |
| 4 | < 100,000 | 0.07 |
| 5 | ≥ 100,000 | 0.03 |

Updated every 720 blocks (~1 day) via exponential moving average of observed spending ages.

**Ring Quality Score formula:**

$$RQS = \sum_{i=1}^{6} W_i \cdot S_i$$

| Component | Weight | What it measures |
|-----------|--------|------------------|
| Age Distribution | 0.25 | Spread across time periods |
| Amount Plausibility | 0.15 | How plausible the decoy amounts are |
| Poison Avoidance | 0.20 | Exclusion of suspicious outputs |
| Temporal Diversity | 0.15 | Block height diversity of decoys |
| Cross-TX Independence | 0.10 | No ring overlap between user's TXs |
| Historical Pattern | 0.15 | Match to real spending patterns |

**Minimum acceptable RQS: 0.85** — if below threshold, wallet retries with different decoys.

**Poison avoidance:** Outputs with `poison_score ≥ 0.7` are excluded from ring selection.

**Chain maturity stages:**

| Stage | Total Outputs | Quality Strictness |
|-------|---------------|-------------------|
| YOUNG | < 50K | Relaxed |
| GROWING | < 200K | Moderate |
| MATURE | < 1M | Strict |
| ESTABLISHED | ≥ 5M | Maximum |

### 13.3 Coinbase Verification Tags

| Tag | Hex | Content |
|-----|-----|---------|
| Model hash | `0xCA` | SHA-256 of the GGUF model, injected into coinbase extra |
| NINA state | `0xCB` | Snapshot of current NINA state (threat level, ring params) |

These tags allow any node to verify that other miners are running the same NINA model and configuration.

---

## 14. Smart Mempool

### 14.1 Design Principle

The Smart Mempool **never rejects valid transactions**. It only reorders priority. Legitimate bots (high frequency, adequate fees) are welcome.

### 14.2 Transaction Classification

| Category | Description |
|----------|-------------|
| `normal` | Regular user transaction |
| `trading_bot` | High frequency, adequate fees (legitimate) |
| `whale` | Very large amount or very high fee |
| `spam_suspect` | High frequency, minimal fees, repetitive patterns |

Classification is based on "structural fingerprint" (size_bucket + fee_bucket), not sender address (which is hidden in CryptoNote).

### 14.3 Priority Formula

| Factor | Weight |
|--------|--------|
| Fee per byte | ~60% (primary) |
| Category bonus/penalty | ~20% |
| Mempool congestion factor | ~10% |
| Recency | ~10% |

### 14.4 Health Levels

| Status | Meaning |
|--------|---------|
| HEALTHY | Normal operation |
| BUSY | Elevated traffic |
| CONGESTED | Degraded performance |
| UNDER_ATTACK | Spam flood detected |

Constants: `MAX_RECENT_TXS = 2000`, `MAX_TX_CACHE = 5000`.

---

## 15. Persistence & Auditability

### 15.1 Dual-Mode Architecture

| Phase | Backend | Location |
|-------|---------|----------|
| Pre-v18 (height < 20,000) | Separate LMDB | `~/.ninacatcoin/nina_state/data.mdb` |
| Post-v18 (height ≥ 20,000) | Blockchain's data.mdb | Via `BlockchainDB::nina_*_put/get` |

Transition at `NINA_V18_ONCHAIN_HEIGHT = 20000`. Post-v18, the separate DB becomes read-only for migration.

### 15.2 LMDB Configuration

| Parameter | Value |
|-----------|-------|
| Initial map size | 512 MB |
| Auto-growth increment | 512 MB |
| Estimated lifespan | ~80 years at ~6 MB/year |

### 15.3 Database Tables

| Table (DBI) | Key | Value |
|-------------|-----|-------|
| `stats` | `"current"` | Serialized `PersistedStatistics` |
| `meta` | `"current"` | height, timestamp, first_run_ts, version |
| `blocks` | height (8 bytes BE) | Serialized `PersistedBlockRecord` |
| `audit` | timestamp (8 bytes BE) | height, event, details |

### 15.4 Data Structures

**PersistedBlockRecord (7 fields):**
`height | timestamp | solve_time | difficulty | previous_difficulty | lwma_prediction_error | anomaly_flags`

**PersistedStatistics (8 fields):**
`total_blocks_processed | total_anomalies_detected | total_attacks_predicted | session_count | average_prediction_accuracy | peer_reputation_average | network_health_average | last_persistence_time`

### 15.5 Decision Audit Trail

Every significant NINA decision is logged with:
- Timestamp
- Reasoning (from Explanation Engine)
- Evidence (metrics and data)
- Outcome (action taken)
- Confidence score

This creates a complete, auditable history of all AI decisions.

---

## 16. Model Training Pipeline

### 16.1 Dataset

| Property | Value |
|----------|-------|
| Total examples | 3,074 |
| Token count | ~317,000 |
| Format | JSONL, Llama 3 chat template |
| Template | system → user → assistant turns |

**8 specialized domains:**

| Domain | Coverage |
|--------|----------|
| Checkpoint validation | Accept/reject/quarantine with real block hashes |
| Peer classification | Spy patterns, Sybil clusters, honest profiles |
| X2/X200 events | Reward multiplier activation, seed evaluation |
| Network defense | Eclipse, selfish mining, timestamp attacks |
| Mining strategy | Fee optimization, block template decisions |
| Protocol rules | 900M emission, unlock times, dust thresholds |
| Node operations | Sync issues, fork resolution, ban scoring |
| General blockchain | RandomX PoW, CryptoNote privacy, ring signatures |

### 16.2 Training Configuration

| Parameter | Value |
|-----------|-------|
| Base model | unsloth/Llama-3.2-3B-Instruct |
| Method | QLoRA (4-bit quantization + LoRA) |
| LoRA rank (r) | 32 |
| LoRA alpha | 64 |
| Dropout | 0.05 |
| Target modules | 7 (q_proj, k_proj, v_proj, o_proj, gate_proj, up_proj, down_proj) |
| Batch size | 1 (effective 8 with gradient accumulation) |
| Learning rate | 2e-4 |
| Epochs | 3 (1,095 steps) |
| Hardware | NVIDIA GTX 1080 Ti (11 GB VRAM) |
| Training time | 2 hours 20 minutes |
| Final loss | 0.76 |
| Eval loss | 0.688 |

### 16.3 GGUF Conversion

```
LoRA adapter (nina_llama3_ft/)
    │
    ▼
Merge with base model (nina_llama3_merged/)
    │
    ▼
convert_hf_to_gguf.py → F16 GGUF (5.99 GB)
    │
    ▼
llama-quantize Q4_K_M → Q4_K_M GGUF (1.88 GB, 5.01 BPW)
```

### 16.4 Data Collection

The `NinaBlockDataLogger` module continuously writes block data to `~/.ninacatcoin/ml_training_data.csv`:
- height, timestamp, solve_time, difficulty, tx_count, block_size
- Thread-safe append mode
- Used as input for future training iterations

---

## 17. Event System V2

Activated at `NINA_EVENT_V2_HEIGHT = 20000`.

### 17.1 X2/X200 Reward Events

Special block reward multipliers triggered by deterministic blockchain conditions.

**Multi-hash seed:** Concatenation of the last 10 block hashes, providing unpredictable but deterministic seed material.

$$seed = H(block_{n-1} \| block_{n-2} \| ... \| block_{n-10})$$

### 17.2 Spike Freeze Mechanism

When difficulty spikes ≥ 50% above recent average, reward events are frozen:

| Parameter | Value |
|-----------|-------|
| Spike threshold | 3/2 = 1.5x (50% above baseline) |
| Recent window | Last 5 blocks |
| Baseline window | Last 20 blocks |
| Freeze duration | 5–30 blocks (5 min + 26 range) |

This prevents miners from manipulating difficulty to trigger favorable reward events.

### 17.3 NINA Local Fork Parameters

Activated at height 15,000:

| Parameter | Value |
|-----------|-------|
| Recent window | 5 blocks |
| Older window | 15 blocks |
| Smoothing factor | 0.3 |
| Max adjustment | ±5% |

---

## 18. Auto-Update System

### 18.1 Monitoring

`NinaIAAutoUpdate` polls GitHub every 6 hours (30 minutes in urgent mode):

| Source | URL |
|--------|-----|
| Compare API | `https://api.github.com/repos/ninacatcoin/ninacatcoin/compare/` |
| Config file | `.../master/src/cryptonote_config.h` |
| Hardforks | `.../master/src/hardforks/hardforks.cpp` |

### 18.2 Change Classification

| Type | Severity | Description |
|------|----------|-------------|
| HARD_FORK | 5 (critical) | New fork height or version |
| SECURITY_FIX | 4-5 | Security vulnerability patch |
| SOFT_FORK | 3 | Consensus parameter change |
| PERFORMANCE | 2 | Optimization |
| FEATURE | 2 | New functionality |
| CONFIG_CHANGE | 1 | Configuration update |

### 18.3 Decision Matrix

| Decision | When |
|----------|------|
| `UPDATE_NOW` | Critical security fix or imminent hard fork |
| `UPDATE_SCHEDULED` | Wait for low-activity window |
| `UPDATE_BEFORE_FORK` | Before fork height (500 block margin) |
| `UPDATE_OPTIONAL` | Non-critical, user decides |
| `UPDATE_SKIP` | Pre-release or irrelevant |
| `UPDATE_DEFERRED` | Node busy (syncing) |

### 18.4 Timing Constants

| Constant | Value |
|----------|-------|
| Check interval | 21,600s (6 hours) |
| Urgent check interval | 1,800s (30 minutes) |
| Startup delay | 180s (3 minutes) |
| Max sync wait | 3,600s (1 hour) |
| Pre-fork margin | 500 blocks |

The system can compile, install, and restart the daemon without human intervention.

---

## 19. Network Optimization

`NinaNetworkOptimizer` manages P2P connections intelligently:

**Peer quality scoring:**
- Latency (lower = better)
- Reliability (blocks_relayed / total)
- Protocol version (newer = better)
- Chain height (closer to tip = better)

**Automatic adjustments:**
- Peer count based on available bandwidth
- Priority connections to full-chain nodes during initial sync
- Disconnect low-quality peers to make room for better ones

---

## 20. Performance Characteristics

### 20.1 Memory Usage

| Component | RAM |
|-----------|-----|
| Model (Q4_K_M) | ~2.0 GB |
| Context (2048 tokens) | ~0.5 GB |
| NINA modules (all) | ~50 MB |
| LMDB cache | ~10 MB |
| **Total NINA overhead** | **~2.5 GB** |

### 20.2 Inference Latency

| Task | Temperature | Tokens | Estimated Time (CPU) |
|------|-------------|--------|---------------------|
| Decision (greedy) | 0.0 | ~50 | ~2-5s |
| Analysis (sampled) | 0.3 | ~256 | ~10-30s |
| Periodic summary | 0.3 | ~256 | ~10-30s |

Measured on consumer CPUs (4-8 threads). NINA only queries the LLM for rare/important events; high-frequency events use embedded ML (microseconds).

### 20.3 Disk Usage

| Component | Size |
|-----------|------|
| GGUF model | 1.88 GB |
| LMDB state DB | 512 MB initial, ~6 MB/year growth |
| Training CSV | ~1 MB/year |
| **Total** | **~2.4 GB** |

---

## 21. Security Analysis

### 21.1 Threat Model

| Threat | Mitigation |
|--------|-----------|
| Model tampering | SHA-256 verification at startup; coinbase tag `0xCA` |
| Prompt injection via crafted blocks | LLM Firewall blocks 45+ injection patterns |
| LLM outputs financial commands | Output filter blocks 30+ patterns; address/key detection |
| Constitutional bypass | `static_assert` at compile time; no runtime toggle |
| Sybil attack on ring quality | Adaptive ring sizing; threat assessment window 100 blocks |
| Spy node surveillance | 5 active countermeasures; 8-type classification |
| Eclipse attack | Node Protector immune system; isolation recovery |
| Model supply chain | Primary + fallback download URLs; cryptographic hash verification |

### 21.2 CPU-Only Requirement

NINA mandates `n_gpu_layers = 0` for two reasons:
1. **Determinism**: GPU floating-point is non-deterministic across hardware; CPU guarantees bit-identical results
2. **Accessibility**: Every node operator can run NINA without a GPU

### 21.3 Greedy Sampling Guarantee

With temperature=0.0 and greedy sampling, the model always selects the token with the highest logit. Given:
- Same model weights (verified by SHA-256)
- Same input (deterministic event data)
- Same inference code (same binary)

The output is mathematically identical across all nodes. This is essential for ring size consensus.

---

## 22. Future Work

1. **On-chain persistence (v18)**: Migrate all NINA state from separate LMDB to the blockchain's own database
2. **Model v3**: Larger training dataset from accumulated block data (CSV logger)
3. **Federated learning**: Nodes share anonymized learning without raw data
4. **Multi-model ensemble**: Specialized smaller models per task domain
5. **Hardware attestation**: Proof that nodes run unmodified NINA binary

---

## 23. References

1. Cao, T., et al. "Friend or Foe? Identifying Spy Nodes in the Monero Peer-to-Peer Network." 2025.
2. Shi, Y., et al. "Eclipse Attacks on Cryptocurrency P2P Networks." NDSS 2025.
3. Meta AI. "LLaMA: Open and Efficient Foundation Language Models." 2024.
4. Hu, E., et al. "LoRA: Low-Rank Adaptation of Large Language Models." ICLR 2022.
5. Dettmers, T., et al. "QLoRA: Efficient Finetuning of Quantized LLMs." NeurIPS 2023.
6. llama.cpp — Inference of Meta's LLaMA model in C/C++. https://github.com/ggml-org/llama.cpp
7. NinaCatCoin Project. https://github.com/ninacatcoin
8. CryptoNote Standard. https://cryptonote.org
9. RandomX — Proof of work algorithm. https://github.com/tevador/RandomX

---

**SHA-256 of this document:** To be computed upon final publication.

**License:** This document is released under CC BY-SA 4.0.
