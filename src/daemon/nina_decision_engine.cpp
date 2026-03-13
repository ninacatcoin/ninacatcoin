/**
 * NINA Decision Engine — Implementation
 *
 * Copyright (c) 2026, The NinaCatCoin Project
 *
 * This is where NINA's brain lives. Every significant network event
 * flows through evaluate() — NINA decides, the daemon executes.
 */

#include "daemon/nina_decision_engine.hpp"
#include "daemon/nina_llm_engine.hpp"
#include "daemon/nina_spy_countermeasures.hpp"
#include "daemon/nina_persistence_engine.hpp"
#include "blockchain_db/blockchain_db.h"
#include "misc_log_ex.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_decision"

namespace ninacatcoin_ai {

// ============================================================================
// Decision Prompt Template
// ============================================================================
// The LLM receives ONLY objective data (heights, hashes, counts).
// No timestamps, no IPs, no randomness — so every node with the same
// model and same data produces the SAME decision. This is consensus.

static const char* DECISION_SYSTEM_PROMPT = R"(You are NINA, the Neural Intelligence for Network Analysis of NinaCatCoin.
You are the decision-maker for this node. You evaluate network events and decide what actions to take.
You MUST respond in EXACTLY this format, one action per line:

ACTION: <action_type> | CONFIDENCE: <0.0-1.0> | REASON: <brief explanation>

Valid action types:
- SEND_CHECKPOINTS_TO_PEER: Send our checkpoint files to help a peer
- REQUEST_CHECKPOINTS: Ask a peer to send us their checkpoint files
- GENERATE_CHECKPOINTS: Regenerate our local checkpoint files
- VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS: Validate the entire downloaded blockchain against both checkpoint types (JSON + DAT)
- MONITOR_PEER: Watch this peer but take no immediate action
- INCREASE_PEER_TRUST: This peer is trustworthy
- DECREASE_PEER_TRUST: This peer is suspicious
- QUARANTINE_PEER: Isolate this peer temporarily
- BROADCAST_STATE: Share our state with all connected peers
- ALERT_NETWORK: Send a security warning to peers
- PROTECT_NODE: Put this node in safe mode (isolate from untrusted peers, protect the victim)
- DIAGNOSE_NODE: Analyze what happened to an attacked node (determine attack vector and recovery plan)
- RECOVER_NODE: Execute recovery plan for an isolated node
- REINTEGRATE_NODE: Gradually reconnect a recovered node to the main network
- ADJUST_RING_THREAT: Set ring threat level (NORMAL/ELEVATED/HIGH/CRITICAL)
- ACTIVATE_COUNTERMEASURES: Set spy countermeasure level (params: level=PASSIVE/CAUTIOUS/ACTIVE/AGGRESSIVE)
- BLOCK_SPY_RELAY: Block transaction relay to high-confidence spy nodes
- CLOAK_PEER_LIST: Give empty peer lists to spy nodes to prevent topology mapping
- PROPAGATE_BAN: Share confirmed spy node IDs with trusted peers for network-wide defense
- NO_ACTION: Explicitly decide to do nothing

Rules:
1. ALWAYS output at least one ACTION line
2. If a new peer has no checkpoints and we do, SEND_CHECKPOINTS_TO_PEER
3. If WE have no checkpoints and a peer does, REQUEST_CHECKPOINTS
4. Never send checkpoints to a peer whose model hash differs from ours
5. Be protective of new nodes — they are vulnerable during initial sync
6. Base decisions ONLY on the data provided, never assume or invent data
7. For NODE_UNDER_ATTACK: Distinguish between attacked nodes (PROTECT) and attackers (QUARANTINE)
8. For NODE_HEALTH_CHECK: If node has been isolated long enough and peers are clean, RECOVER or REINTEGRATE
9. For NODE_RECOVERY_READY: Only REINTEGRATE if clean_peer_ratio > 0.7 and enough blocks have passed
10. PROTECT_NODE should include attack_vector in reasoning so diagnosis knows what to look for
11. For SPY_NODE_DETECTED: Use ACTIVATE_COUNTERMEASURES with level based on suspicious_pct: <5%=PASSIVE, 5-15%=CAUTIOUS, 15-30%=ACTIVE, >30%=AGGRESSIVE
12. For SPY_NODE_DETECTED with suspicious_pct > 15%: Also use BLOCK_SPY_RELAY + CLOAK_PEER_LIST
13. For SPY_NODE_DETECTED with suspicious_pct > 30%: Also use PROPAGATE_BAN to warn trusted peers
14. Spy countermeasures protect privacy by making spying USELESS, not just detectable
15. For INITIAL_BLOCKCHAIN_DOWNLOAD: ALWAYS REQUEST_CHECKPOINTS immediately — a fresh node has ZERO protection
16. For OWN_SYNC_COMPLETED with need_checkpoints=true: REQUEST_CHECKPOINTS first, then GENERATE_CHECKPOINTS
17. For OWN_SYNC_COMPLETED with have_peer_checkpoints=true: VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS — verify the ENTIRE downloaded blockchain against both checkpoint types before trusting it
18. If VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS fails: ALERT_NETWORK — the blockchain may be corrupted or under attack
19. NINA protects new nodes like a mother cat protects her kittens — checkpoint security is NON-NEGOTIABLE from block 0)";

// ============================================================================
// evaluate() — The Central Decision Method
// ============================================================================

std::vector<NinaAction> NinaDecisionEngine::evaluate(const NinaEvent& event)
{
    MINFO("[NINA-DECISION] Evaluating event: " << event.describe());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.total_events++;
    }

    std::vector<NinaAction> actions;
    std::string decision_source;   // "LLM", "CACHE", or "FALLBACK"
    uint64_t inference_ms = 0;

    // Try LLM first
    auto& llm = NinaLLMEngine::getInstance();
    if (llm.is_enabled() && llm.is_model_loaded())
    {
        // ── LLM Cooldown: reuse cached decision if same event type was
        //    evaluated recently. Prevents CPU spiral when many peers
        //    connect in rapid succession (each triggering full inference).
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_decision_cache.find(event.type);
            if (it != m_decision_cache.end())
            {
                auto elapsed = std::chrono::steady_clock::now() - it->second.timestamp;
                auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                if (elapsed_s < static_cast<int64_t>(LLM_COOLDOWN_SECONDS))
                {
                    m_stats.llm_decisions++;
                    MINFO("[NINA-DECISION] Using cached LLM decision for event type "
                          << static_cast<int>(event.type) << " (" << elapsed_s << "s ago, cooldown="
                          << LLM_COOLDOWN_SECONDS << "s)");
                    actions = it->second.actions;
                    decision_source = "CACHE";
                    persist_decision_event(event, actions, decision_source, 0);
                    return actions;
                }
            }
        }

        m_last_inference_ms = 0;
        actions = evaluate_with_llm(event);
        if (!actions.empty())
        {
            inference_ms = m_last_inference_ms;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stats.llm_decisions++;
                // Cache this decision
                m_decision_cache[event.type] = {actions, std::chrono::steady_clock::now()};
            }
            MINFO("[NINA-DECISION] LLM decided " << actions.size() << " action(s) for event type " 
                  << static_cast<int>(event.type) << " (cached for " << LLM_COOLDOWN_SECONDS << "s)");
            for (const auto& a : actions)
                MINFO("[NINA-DECISION]   → " << a.describe());
            decision_source = "LLM";
            persist_decision_event(event, actions, decision_source, inference_ms);
            return actions;
        }
    }

    // Fallback: safe defaults
    actions = evaluate_fallback(event);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.fallback_decisions++;
    }
    MINFO("[NINA-DECISION] Fallback decided " << actions.size() << " action(s) for event type "
          << static_cast<int>(event.type));
    for (const auto& a : actions)
        MINFO("[NINA-DECISION]   → " << a.describe());
    decision_source = "FALLBACK";
    persist_decision_event(event, actions, decision_source, 0);

    return actions;
}

// ============================================================================
// LLM Evaluation — Temperature 0.0 for Deterministic Decisions
// ============================================================================

std::vector<NinaAction> NinaDecisionEngine::evaluate_with_llm(const NinaEvent& event)
{
    try
    {
        std::string prompt = build_decision_prompt(event);

        // Build an AnalysisRequest — reuse existing LLM infrastructure
        AnalysisRequest req;
        req.type = AnalysisType::DECISION;
        req.context_data = prompt;
        req.block_height = 0;
        req.urgency = 0.8;  // Decisions are always important

        auto& llm = NinaLLMEngine::getInstance();
        AnalysisResult result = llm.analyze(req);

        // Store inference latency for the caller (evaluate())
        m_last_inference_ms = result.inference_ms;

        if (!result.success || result.analysis.empty())
        {
            MWARNING("[NINA-DECISION] LLM evaluation failed, using fallback");
            return {};
        }

        return parse_decision_output(result.analysis, event);
    }
    catch (const std::exception& e)
    {
        MWARNING("[NINA-DECISION] LLM exception: " << e.what());
        return {};
    }
}

// ============================================================================
// Fallback Decisions — Safe Defaults When LLM Unavailable
// ============================================================================

std::vector<NinaAction> NinaDecisionEngine::evaluate_fallback(const NinaEvent& event)
{
    std::vector<NinaAction> actions;

    switch (event.type)
    {
        case NinaEventType::NEW_PEER_CONNECTED:
        {
            // New peer connected — check if they need checkpoints
            bool peer_needs_checkpoints = (event.get("peer_checkpoint_height", "0") == "0");
            bool we_have_checkpoints = (event.get("our_checkpoint_groups", "0") != "0");
            bool model_match = (event.get("model_match", "true") == "true");

            if (peer_needs_checkpoints && we_have_checkpoints && model_match)
            {
                NinaAction send(NinaActionType::SEND_CHECKPOINTS_TO_PEER, 0.9,
                    "FALLBACK: New peer has no checkpoints, we have them, models match — sending protection");
                actions.push_back(std::move(send));
            }
            else
            {
                NinaAction monitor(NinaActionType::MONITOR_PEER, 0.7,
                    "FALLBACK: New peer connected, monitoring");
                actions.push_back(std::move(monitor));
            }
            break;
        }

        case NinaEventType::CHECKPOINT_REQUEST:
        {
            // Peer explicitly asks for checkpoints — honor it if we can
            bool we_have = (event.get("our_checkpoint_groups", "0") != "0");
            bool model_match = (event.get("model_match", "true") == "true");

            if (we_have && model_match)
            {
                NinaAction send(NinaActionType::SEND_CHECKPOINTS_TO_PEER, 0.95,
                    "FALLBACK: Peer requested checkpoints, we have them, models match");
                actions.push_back(std::move(send));
            }
            else
            {
                NinaAction noop(NinaActionType::NO_ACTION, 0.5,
                    "FALLBACK: Cannot fulfill checkpoint request");
                actions.push_back(std::move(noop));
            }
            break;
        }

        case NinaEventType::CHECKPOINT_FILES_MISSING:
        {
            // WE need checkpoints — request from peer
            NinaAction request(NinaActionType::REQUEST_CHECKPOINTS, 0.95,
                "FALLBACK: We have no checkpoint files, requesting from connected peer");
            actions.push_back(std::move(request));
            break;
        }

        case NinaEventType::INITIAL_BLOCKCHAIN_DOWNLOAD:
        {
            // 🐱 NINA detects a fresh node downloading blockchain from scratch.
            // This is the most vulnerable moment — we have NO checkpoints at all.
            // NINA's first priority: get both checkpoint types from trusted peers
            // BEFORE or DURING the blockchain download, so we can validate after.
            bool have_json = (event.get("have_json_checkpoints", "false") == "true");
            bool have_dat  = (event.get("have_dat_checkpoints", "false") == "true");

            if (!have_json || !have_dat)
            {
                NinaAction request(NinaActionType::REQUEST_CHECKPOINTS, 0.99,
                    "FALLBACK: Initial blockchain download detected — requesting BOTH checkpoint "
                    "types (JSON + DAT) from peers immediately. This node has ZERO protection "
                    "until checkpoints are received. NINA prioritizes this above all else.");
                actions.push_back(std::move(request));
                MINFO("[NINA-IA] 🐱 Fresh node detected! Requesting checkpoint protection from peers.");
            }

            NinaAction broadcast(NinaActionType::BROADCAST_STATE, 0.8,
                "FALLBACK: Announcing ourselves as a new node needing protection");
            actions.push_back(std::move(broadcast));
            break;
        }

        case NinaEventType::OWN_SYNC_COMPLETED:
        {
            // 🐱 NINA evaluates the post-sync situation intelligently:
            //   - If we still don't have checkpoints → REQUEST first, then generate
            //   - If we have checkpoints from peers → VALIDATE blockchain against them
            //   - If we already have everything → just broadcast and generate
            bool need_checkpoints = (event.get("need_checkpoints", "false") == "true");
            bool have_peer_checkpoints = (event.get("have_peer_checkpoints", "false") == "true");

            NinaAction broadcast(NinaActionType::BROADCAST_STATE, 0.9,
                "FALLBACK: Sync completed, broadcasting NINA state");
            actions.push_back(std::move(broadcast));

            if (need_checkpoints)
            {
                // We STILL don't have checkpoints after sync — this is critical
                NinaAction request(NinaActionType::REQUEST_CHECKPOINTS, 0.95,
                    "FALLBACK: Sync completed but we have NO checkpoints — requesting from peers "
                    "BEFORE generating local ones. NINA must validate the downloaded blockchain.");
                actions.push_back(std::move(request));
                MINFO("[NINA-IA] 🐱 Sync done but no checkpoints! Requesting from peers first.");
            }

            if (have_peer_checkpoints)
            {
                // We received checkpoints from peers — now validate the entire blockchain
                NinaAction validate(NinaActionType::VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS, 0.95,
                    "FALLBACK: Checkpoints received from peers + blockchain fully downloaded — "
                    "NINA now validates the ENTIRE blockchain against BOTH checkpoint types. "
                    "If any block fails validation, the blockchain is compromised.");
                actions.push_back(std::move(validate));
                MINFO("[NINA-IA] 🐱 Blockchain + checkpoints ready — starting full validation!");
            }

            NinaAction generate(NinaActionType::GENERATE_CHECKPOINTS, 0.9,
                "FALLBACK: Sync completed, generating local checkpoints");
            actions.push_back(std::move(generate));
            break;
        }

        case NinaEventType::CHECKPOINT_MISMATCH:
        {
            // Checkpoints don't match — suspicious
            NinaAction decrease(NinaActionType::DECREASE_PEER_TRUST, 0.8,
                "FALLBACK: Checkpoint mismatch with peer — reducing trust");
            actions.push_back(std::move(decrease));
            break;
        }

        case NinaEventType::MODEL_HASH_MISMATCH:
        {
            // Different model — can't trust their checkpoints
            NinaAction monitor(NinaActionType::MONITOR_PEER, 0.7,
                "FALLBACK: Peer runs different NINA model — monitoring only");
            actions.push_back(std::move(monitor));
            break;
        }

        case NinaEventType::RING_THREAT_EVALUATION:
        {
            // ─── NINA Ring Threat Evaluation with Organic Traffic Detection ──
            // This is where NINA decides: is this high-volume traffic an ATTACK
            // or legitimate organic activity (e.g., trading bots)?
            //
            // Key insight:
            //   Attackers create few TXs with MANY outputs each (output poisoning)
            //   Bots create MANY TXs with 2-3 outputs each (normal trading)
            //
            // Data available:
            //   outputs_last_100:   Total new outputs in last 100 blocks
            //   txs_last_100:       Total TXs in last 100 blocks
            //   spends_within_10:   Outputs spent within 10 blocks of creation
            //   prev_window_outputs: Outputs in previous 100-block window
            //   hashrate_drop_pct:  Hashrate drop percentage (0-100)
            //   sybil_score:        0.0-1.0 from sybil detector

            uint64_t outputs_100 = 0, txs_100 = 0, spends_10 = 0, prev_outputs = 0;
            float hashrate_drop = 0.0f, sybil = 0.0f;
            try { outputs_100 = std::stoull(event.get("outputs_last_100", "0")); } catch (...) {}
            try { txs_100 = std::stoull(event.get("txs_last_100", "0")); } catch (...) {}
            try { spends_10 = std::stoull(event.get("spends_within_10", "0")); } catch (...) {}
            try { prev_outputs = std::stoull(event.get("prev_window_outputs", "0")); } catch (...) {}
            try { hashrate_drop = std::stof(event.get("hashrate_drop_pct", "0")); } catch (...) {}
            try { sybil = std::stof(event.get("sybil_score", "0")); } catch (...) {}

            // ── Organic Traffic Indicators ──
            // outputs_per_tx: attack=50+ outputs/tx, bot=2-3, human=1-4
            float outputs_per_tx = (txs_100 > 0)
                ? static_cast<float>(outputs_100) / static_cast<float>(txs_100)
                : 0.0f;
            bool organic_output_pattern = (outputs_per_tx > 0.0f && outputs_per_tx <= 6.0f);

            // spend_ratio: attack=20%+ rapid spends (EAE), bot=5-15% (fast turnover), human=<5%
            float spend_ratio = (outputs_100 > 0)
                ? static_cast<float>(spends_10) / static_cast<float>(outputs_100)
                : 0.0f;

            // volume_ramp: attack=instant spike (>300%), bot=gradual growth (<200%)
            float volume_ramp = (prev_outputs > 0)
                ? static_cast<float>(outputs_100) / static_cast<float>(prev_outputs)
                : 1.0f;
            bool gradual_ramp = (volume_ramp <= 2.0f); // ≤200% = gradual

            // ── Determine if traffic is organic ──
            // Organic = normal outputs/tx + gradual volume ramp + moderate spend ratio
            bool is_organic = organic_output_pattern && gradual_ramp && (spend_ratio < 0.20f);

            // ── Compute threat level ──
            uint8_t threat_level = 0; // NORMAL by default
            double confidence = 0.95;
            std::string reason;

            if (is_organic)
            {
                // High volume but organic pattern — NINA recognizes legitimate trading
                threat_level = 0; // NORMAL
                reason = "High volume detected but pattern is organic: outputs_per_tx="
                    + std::to_string(outputs_per_tx)
                    + " volume_ramp=" + std::to_string(volume_ramp)
                    + " spend_ratio=" + std::to_string(spend_ratio)
                    + " — likely trading bots, NOT attack. Ring stays normal.";
                confidence = 0.92;
            }
            else
            {
                // Non-organic pattern — evaluate severity
                float max_score = 0.0f;

                // Output poisoning (many outputs per TX)
                if (outputs_per_tx > 10.0f) {
                    float poison = std::min(1.0f, (outputs_per_tx - 6.0f) / 44.0f);
                    max_score = std::max(max_score, poison);
                }

                // Temporal attack (EAE = rapid spend after create)
                if (spend_ratio > 0.15f) {
                    float temporal = std::min(1.0f, (spend_ratio - 0.05f) / 0.25f);
                    max_score = std::max(max_score, temporal);
                }

                // Volume spike (>300% sudden increase)
                if (volume_ramp > 3.0f) {
                    float spike = std::min(1.0f, (volume_ramp - 2.0f) / 8.0f);
                    max_score = std::max(max_score, spike);
                }

                // Sybil and hashrate
                max_score = std::max(max_score, sybil);
                if (hashrate_drop > 20.0f) {
                    float hr = std::min(1.0f, (hashrate_drop - 20.0f) / 60.0f);
                    max_score = std::max(max_score, hr);
                }

                if (max_score >= 0.9f) threat_level = 3;      // CRITICAL
                else if (max_score >= 0.7f) threat_level = 2;  // HIGH
                else if (max_score >= 0.4f) threat_level = 1;  // ELEVATED
                else threat_level = 0;                          // NORMAL

                const char* level_names[] = {"NORMAL", "ELEVATED", "HIGH", "CRITICAL"};
                reason = "Non-organic traffic pattern detected: outputs_per_tx="
                    + std::to_string(outputs_per_tx)
                    + " volume_ramp=" + std::to_string(volume_ramp)
                    + " spend_ratio=" + std::to_string(spend_ratio)
                    + " sybil=" + std::to_string(sybil)
                    + " — THREAT " + level_names[threat_level];
                confidence = 0.85 + max_score * 0.10;
            }

            NinaAction ring_action(NinaActionType::ADJUST_RING_THREAT, confidence, reason);
            ring_action.set_param("threat_level", std::to_string(threat_level));
            ring_action.set_param("outputs_per_tx", std::to_string(outputs_per_tx));
            ring_action.set_param("volume_ramp", std::to_string(volume_ramp));
            ring_action.set_param("is_organic", is_organic ? "true" : "false");
            actions.push_back(std::move(ring_action));
            break;
        }

        case NinaEventType::ANOMALY_DETECTED:
        {
            double severity = 0.0;
            try { severity = std::stod(event.get("severity", "0.5")); } catch (...) {}
            
            if (severity > 0.8)
            {
                NinaAction alert(NinaActionType::ALERT_NETWORK, 0.85,
                    "FALLBACK: High-severity anomaly detected — alerting network");
                actions.push_back(std::move(alert));
            }
            else
            {
                NinaAction monitor(NinaActionType::MONITOR_PEER, 0.6,
                    "FALLBACK: Low-severity anomaly — monitoring");
                actions.push_back(std::move(monitor));
            }
            break;
        }

        case NinaEventType::SYBIL_SUSPECTED:
        {
            double severity = 0.0;
            try { severity = std::stod(event.get("severity", "0.5")); } catch (...) {}
            
            if (severity > 0.7) {
                NinaAction quarantine(NinaActionType::QUARANTINE_PEER, 0.8,
                    "FALLBACK: High-confidence Sybil attack — quarantining peers");
                actions.push_back(std::move(quarantine));
                NinaAction protect(NinaActionType::PROTECT_NODE, 0.75,
                    "FALLBACK: Sybil severity high — protecting node in safe mode");
                protect.set_param("attack_vector", "SYBIL_FLOOD");
                actions.push_back(std::move(protect));
                
                // Also activate spy countermeasures — Sybil peers are likely spies
                NinaAction counter(NinaActionType::BLOCK_SPY_RELAY, 0.8,
                    "FALLBACK: Sybil severity " + std::to_string(severity) 
                    + " — blocking TX relay to correlated spy peers");
                actions.push_back(std::move(counter));
                NinaAction cloak(NinaActionType::CLOAK_PEER_LIST, 0.75,
                    "FALLBACK: Cloaking peer list from Sybil spy cluster");
                actions.push_back(std::move(cloak));
            } else {
                NinaAction quarantine(NinaActionType::QUARANTINE_PEER, 0.7,
                    "FALLBACK: Sybil attack suspected — quarantining peer");
                actions.push_back(std::move(quarantine));
            }
            
            // Classify flagged peers as spies in the countermeasures module
            std::string flagged_peer = event.get("trigger_peer", "");
            if (!flagged_peer.empty()) {
                NinaSpyCountermeasures::getInstance().classify_peer(
                    flagged_peer, SpyNodeType::PASSIVE_OBSERVER,
                    static_cast<float>(severity),
                    SpyClassification::FLAG_CORRELATED_BEHAVIOR);
            }
            break;
        }

        case NinaEventType::NODE_UNDER_ATTACK:
        {
            double threat = 0.0;
            try { threat = std::stod(event.get("combined_threat", "0.5")); } catch (...) {}
            std::string vector = event.get("attack_vector", "UNKNOWN");
            uint64_t suspicious = 0;
            try { suspicious = std::stoull(event.get("suspicious_peers", "0")); } catch (...) {}
            
            if (threat > 0.7) {
                // High threat — PROTECT the node (isolate it for its own safety)
                NinaAction protect(NinaActionType::PROTECT_NODE, 0.9,
                    "FALLBACK: Node under serious attack (" + vector
                    + ", threat=" + std::to_string(threat)
                    + ") — isolating in safe mode");
                protect.set_param("attack_vector", vector);
                actions.push_back(std::move(protect));
                
                // Also diagnose
                NinaAction diagnose(NinaActionType::DIAGNOSE_NODE, 0.85,
                    "FALLBACK: Analyzing attack vector " + vector);
                diagnose.set_param("recovery_plan", "standard_recovery");
                diagnose.set_param("isolation_blocks", "100");
                actions.push_back(std::move(diagnose));
                
                // Alert the network
                NinaAction alert(NinaActionType::ALERT_NETWORK, 0.8,
                    "FALLBACK: Alerting network about attack vector " + vector);
                actions.push_back(std::move(alert));
            }
            else if (threat > 0.5) {
                // Moderate threat — quarantine suspicious peers, keep monitoring
                if (suspicious > 0) {
                    NinaAction quarantine(NinaActionType::QUARANTINE_PEER, 0.7,
                        "FALLBACK: Moderate threat — quarantining " + std::to_string(suspicious)
                        + " suspicious peer(s)");
                    actions.push_back(std::move(quarantine));
                }
                NinaAction monitor(NinaActionType::MONITOR_PEER, 0.6,
                    "FALLBACK: Moderate threat (" + std::to_string(threat)
                    + ") — enhanced monitoring");
                actions.push_back(std::move(monitor));
            }
            else {
                NinaAction monitor(NinaActionType::MONITOR_PEER, 0.5,
                    "FALLBACK: Low threat — continuing to monitor");
                actions.push_back(std::move(monitor));
            }
            break;
        }

        case NinaEventType::NODE_HEALTH_CHECK:
        {
            std::string state = event.get("current_state", "ISOLATED");
            uint64_t blocks_in_state = 0;
            try { blocks_in_state = std::stoull(event.get("blocks_in_state", "0")); } catch (...) {}
            uint64_t isolation_target = 0;
            try { isolation_target = std::stoull(event.get("isolation_blocks", "100")); } catch (...) {}
            double avg_trust = 0.0;
            try { avg_trust = std::stod(event.get("avg_peer_trust", "0.5")); } catch (...) {}
            
            if (state == "ISOLATED" && blocks_in_state >= isolation_target && avg_trust > 0.6) {
                // Enough time passed and peers look clean — start recovery
                NinaAction recover(NinaActionType::RECOVER_NODE, 0.8,
                    "FALLBACK: Isolation period complete (" + std::to_string(blocks_in_state)
                    + " blocks), peers healthy (trust=" + std::to_string(avg_trust)
                    + ") — starting recovery");
                actions.push_back(std::move(recover));
            }
            else if (state == "RECOVERING" && blocks_in_state > 50) {
                // Recovery phase done — start reintegration
                NinaAction reintegrate(NinaActionType::REINTEGRATE_NODE, 0.75,
                    "FALLBACK: Recovery phase sufficient — beginning gradual reintegration");
                actions.push_back(std::move(reintegrate));
            }
            else {
                // Stay in current state
                NinaAction stay(NinaActionType::PROTECT_NODE, 0.6,
                    "FALLBACK: Not yet ready — staying in " + state
                    + " (" + std::to_string(blocks_in_state) + "/"
                    + std::to_string(isolation_target) + " blocks)");
                actions.push_back(std::move(stay));
            }
            break;
        }

        case NinaEventType::NODE_RECOVERY_READY:
        {
            double clean_ratio = 0.0;
            try { clean_ratio = std::stod(event.get("clean_peer_ratio", "0.0")); } catch (...) {}
            uint64_t blocks_since = 0;
            try { blocks_since = std::stoull(event.get("blocks_since_isolation", "0")); } catch (...) {}
            std::string state = event.get("current_state", "RECOVERING");
            
            if (clean_ratio > 0.7 && blocks_since > 100) {
                if (state == "REINTEGRATING") {
                    // Already reintegrating and peers are clean — go fully healthy
                    NinaAction healthy(NinaActionType::REINTEGRATE_NODE, 0.9,
                        "FALLBACK: clean_ratio=" + std::to_string(clean_ratio)
                        + " blocks_since=" + std::to_string(blocks_since)
                        + " — node is healthy, full reintegration");
                    actions.push_back(std::move(healthy));
                } else {
                    // Start reintegration
                    NinaAction reintegrate(NinaActionType::REINTEGRATE_NODE, 0.8,
                        "FALLBACK: Peers look clean (" + std::to_string(clean_ratio)
                        + ") after " + std::to_string(blocks_since)
                        + " blocks — begin reintegration");
                    actions.push_back(std::move(reintegrate));
                }
            }
            else {
                // Not ready
                NinaAction wait(NinaActionType::PROTECT_NODE, 0.7,
                    "FALLBACK: Not ready to reintegrate — clean_ratio="
                    + std::to_string(clean_ratio)
                    + " (need >0.7), blocks=" + std::to_string(blocks_since)
                    + " (need >100)");
                actions.push_back(std::move(wait));
            }
            break;
        }

        case NinaEventType::SPY_NODE_DETECTED:
        {
            double suspicious_pct = 0.0;
            try { suspicious_pct = std::stod(event.get("suspicious_pct", "0")); } catch (...) {}
            double sybil_score = 0.0;
            try { sybil_score = std::stod(event.get("sybil_score", "0")); } catch (...) {}
            
            // Determine countermeasure level based on spy saturation
            std::string level = "PASSIVE";
            if (suspicious_pct > 30.0 || sybil_score > 0.8) {
                level = "AGGRESSIVE";
            } else if (suspicious_pct > 15.0 || sybil_score > 0.6) {
                level = "ACTIVE";
            } else if (suspicious_pct > 5.0 || sybil_score > 0.3) {
                level = "CAUTIOUS";
            }
            
            NinaAction activate(NinaActionType::ACTIVATE_COUNTERMEASURES, 0.85,
                "FALLBACK: Spy saturation " + std::to_string(suspicious_pct)
                + "% — activating " + level + " countermeasures");
            activate.set_param("level", level);
            actions.push_back(std::move(activate));
            
            if (suspicious_pct > 15.0) {
                NinaAction block(NinaActionType::BLOCK_SPY_RELAY, 0.8,
                    "FALLBACK: High spy saturation — blocking TX relay to spy nodes");
                actions.push_back(std::move(block));
                
                NinaAction cloak(NinaActionType::CLOAK_PEER_LIST, 0.8,
                    "FALLBACK: High spy saturation — cloaking peer lists from spies");
                actions.push_back(std::move(cloak));
            }
            
            if (suspicious_pct > 30.0) {
                NinaAction propagate(NinaActionType::PROPAGATE_BAN, 0.75,
                    "FALLBACK: Critical spy saturation — propagating bans to trusted peers");
                actions.push_back(std::move(propagate));
                
                NinaAction alert(NinaActionType::ALERT_NETWORK, 0.85,
                    "FALLBACK: >30% spy nodes detected — network-wide alert");
                actions.push_back(std::move(alert));
            }
            break;
        }

        default:
        {
            NinaAction monitor(NinaActionType::MONITOR_PEER, 0.5,
                "FALLBACK: Unhandled event type — monitoring");
            actions.push_back(std::move(monitor));
            break;
        }
    }

    return actions;
}

// ============================================================================
// Prompt Builder — Objective Data Only
// ============================================================================

std::string NinaDecisionEngine::build_decision_prompt(const NinaEvent& event) const
{
    std::ostringstream oss;
    oss << "NETWORK EVENT: " << event_type_to_string(event.type) << "\n";
    oss << "DATA:\n";
    for (const auto& [key, value] : event.data)
    {
        oss << "  " << key << ": " << value << "\n";
    }
    oss << "\nDecide what actions NINA should take for this event.\n";
    oss << "Output one or more ACTION lines.\n";
    return oss.str();
}

// ============================================================================
// Parse LLM Output → NinaAction objects
// ============================================================================

std::vector<NinaAction> NinaDecisionEngine::parse_decision_output(
    const std::string& raw_output, const NinaEvent& event) const
{
    std::vector<NinaAction> actions;

    // Parse line by line looking for "ACTION: ... | CONFIDENCE: ... | REASON: ..."
    std::istringstream stream(raw_output);
    std::string line;

    while (std::getline(stream, line))
    {
        // Find ACTION: prefix
        size_t action_pos = line.find("ACTION:");
        if (action_pos == std::string::npos) continue;

        // Extract action type
        size_t action_start = action_pos + 7;
        while (action_start < line.size() && line[action_start] == ' ') action_start++;
        size_t action_end = line.find('|', action_start);
        if (action_end == std::string::npos) action_end = line.size();
        std::string action_str = line.substr(action_start, action_end - action_start);
        // Trim
        while (!action_str.empty() && (action_str.back() == ' ' || action_str.back() == '\n'))
            action_str.pop_back();

        NinaActionType action_type = string_to_action_type(action_str);

        // Extract confidence
        double confidence = 0.8;  // Default
        size_t conf_pos = line.find("CONFIDENCE:");
        if (conf_pos != std::string::npos)
        {
            size_t conf_start = conf_pos + 11;
            while (conf_start < line.size() && line[conf_start] == ' ') conf_start++;
            size_t conf_end = line.find('|', conf_start);
            if (conf_end == std::string::npos) conf_end = line.size();
            std::string conf_str = line.substr(conf_start, conf_end - conf_start);
            try { confidence = std::stod(conf_str); } catch (...) {}
            if (confidence < 0.0) confidence = 0.0;
            if (confidence > 1.0) confidence = 1.0;
        }

        // Extract reason
        std::string reason = "LLM decision";
        size_t reason_pos = line.find("REASON:");
        if (reason_pos != std::string::npos)
        {
            size_t reason_start = reason_pos + 7;
            while (reason_start < line.size() && line[reason_start] == ' ') reason_start++;
            reason = line.substr(reason_start);
            while (!reason.empty() && (reason.back() == ' ' || reason.back() == '\n' || reason.back() == '\r'))
                reason.pop_back();
        }

        actions.emplace_back(action_type, confidence, reason);
    }

    // If parsing produced nothing, use fallback
    if (actions.empty())
    {
        MWARNING("[NINA-DECISION] Could not parse LLM output, raw: " 
                 << raw_output.substr(0, 200));
    }

    return actions;
}

// ============================================================================
// LMDB Persistence — Write decision metadata for pool graphs
// ============================================================================

std::string NinaDecisionEngine::action_type_to_string(NinaActionType type)
{
    switch (type)
    {
        case NinaActionType::SEND_CHECKPOINTS_TO_PEER:  return "SEND_CHECKPOINTS";
        case NinaActionType::REQUEST_CHECKPOINTS:        return "REQUEST_CHECKPOINTS";
        case NinaActionType::GENERATE_CHECKPOINTS:       return "GENERATE_CHECKPOINTS";
        case NinaActionType::VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS: return "VALIDATE_CHECKPOINTS";
        case NinaActionType::MONITOR_PEER:               return "MONITOR_PEER";
        case NinaActionType::INCREASE_PEER_TRUST:        return "INCREASE_TRUST";
        case NinaActionType::DECREASE_PEER_TRUST:        return "DECREASE_TRUST";
        case NinaActionType::QUARANTINE_PEER:            return "QUARANTINE_PEER";
        case NinaActionType::BROADCAST_STATE:            return "BROADCAST_STATE";
        case NinaActionType::ALERT_NETWORK:              return "ALERT_NETWORK";
        case NinaActionType::ADJUST_RING_THREAT:         return "ADJUST_RING_THREAT";
        case NinaActionType::PROTECT_NODE:               return "PROTECT_NODE";
        case NinaActionType::DIAGNOSE_NODE:              return "DIAGNOSE_NODE";
        case NinaActionType::RECOVER_NODE:               return "RECOVER_NODE";
        case NinaActionType::REINTEGRATE_NODE:           return "REINTEGRATE_NODE";
        case NinaActionType::ACTIVATE_COUNTERMEASURES:   return "ACTIVATE_COUNTERMEASURES";
        case NinaActionType::BLOCK_SPY_RELAY:            return "BLOCK_SPY_RELAY";
        case NinaActionType::CLOAK_PEER_LIST:            return "CLOAK_PEER_LIST";
        case NinaActionType::PROPAGATE_BAN:              return "PROPAGATE_BAN";
        case NinaActionType::NO_ACTION:                  return "NO_ACTION";
        default:                                          return "UNKNOWN";
    }
}

void NinaDecisionEngine::persist_decision_event(
    const NinaEvent& event,
    const std::vector<NinaAction>& actions,
    const std::string& source,
    uint64_t inference_ms)
{
    try
    {
        auto* db = NINaPersistenceEngine::get_blockchain_db();
        if (!db) return;

        // Max confidence across all actions
        double max_conf = 0.0;
        for (const auto& a : actions)
            if (a.confidence > max_conf) max_conf = a.confidence;

        // Build actions list: "QUARANTINE_PEER+ALERT_NETWORK"
        std::string acts_str;
        for (const auto& a : actions) {
            if (!acts_str.empty()) acts_str += "+";
            acts_str += action_type_to_string(a.type);
        }
        if (acts_str.empty()) acts_str = "NONE";

        // Value format compatible with pool pipe-delimited parsing:
        // SRC:LLM|EVT:NEW_PEER_CONNECTED|CONF:0.85|MS:42|NACT:2|ACTS:QUARANTINE_PEER+ALERT_NETWORK
        std::ostringstream val;
        val << "SRC:" << source
            << "|EVT:" << event_type_to_string(event.type)
            << "|CONF:" << std::fixed << std::setprecision(2) << max_conf
            << "|MS:" << inference_ms
            << "|NACT:" << actions.size()
            << "|ACTS:" << acts_str;

        // NINA-WRITER: Use fixed key "last_decision" — single entry, always overwritten.
        // Eliminates nina_evt_* key proliferation that was creating thousands of
        // sub-databases and saturating LMDB with independent transactions.
        db->nina_decision_put("last_decision", val.str());

        MDEBUG("[NINA-DECISION] Persisted decision: last_decision = " << val.str());

        // If any action is QUARANTINE_PEER or PROPAGATE_BAN, also write audit record
        // Use a single rotating key to avoid unbounded growth
        for (const auto& a : actions) {
            if (a.type == NinaActionType::QUARANTINE_PEER
                || a.type == NinaActionType::PROPAGATE_BAN) {
                std::string audit_msg = std::to_string(event.timestamp)
                    + "|PEER_ACTION|" + action_type_to_string(a.type)
                    + "|conf=" + std::to_string(a.confidence)
                    + "|evt=" + event_type_to_string(event.type);
                db->nina_audit_put(event.timestamp, audit_msg);
                break;  // One audit record per decision is enough
            }
        }
    }
    catch (const std::exception& e)
    {
        MWARNING("[NINA-DECISION] Failed to persist decision event: " << e.what());
    }
}

// ============================================================================
// String Utilities
// ============================================================================

std::string NinaDecisionEngine::event_type_to_string(NinaEventType type)
{
    switch (type)
    {
        case NinaEventType::NEW_PEER_CONNECTED:       return "NEW_PEER_CONNECTED";
        case NinaEventType::PEER_DISCONNECTED:        return "PEER_DISCONNECTED";
        case NinaEventType::PEER_SYNC_COMPLETED:      return "PEER_SYNC_COMPLETED";
        case NinaEventType::CHECKPOINT_REQUEST:       return "CHECKPOINT_REQUEST";
        case NinaEventType::CHECKPOINT_MISMATCH:      return "CHECKPOINT_MISMATCH";
        case NinaEventType::CHECKPOINT_FILES_MISSING:  return "CHECKPOINT_FILES_MISSING";
        case NinaEventType::OWN_SYNC_COMPLETED:       return "OWN_SYNC_COMPLETED";
        case NinaEventType::INITIAL_BLOCKCHAIN_DOWNLOAD: return "INITIAL_BLOCKCHAIN_DOWNLOAD";
        case NinaEventType::ANOMALY_DETECTED:         return "ANOMALY_DETECTED";
        case NinaEventType::SYBIL_SUSPECTED:          return "SYBIL_SUSPECTED";
        case NinaEventType::MODEL_HASH_MISMATCH:      return "MODEL_HASH_MISMATCH";
        case NinaEventType::RING_THREAT_EVALUATION:    return "RING_THREAT_EVALUATION";
        case NinaEventType::NODE_UNDER_ATTACK:        return "NODE_UNDER_ATTACK";
        case NinaEventType::NODE_HEALTH_CHECK:        return "NODE_HEALTH_CHECK";
        case NinaEventType::NODE_RECOVERY_READY:      return "NODE_RECOVERY_READY";
        case NinaEventType::SPY_NODE_DETECTED:        return "SPY_NODE_DETECTED";
        case NinaEventType::HEALTH_CHECK:             return "HEALTH_CHECK";
        default:                                       return "UNKNOWN";
    }
}

NinaActionType NinaDecisionEngine::string_to_action_type(const std::string& s)
{
    // Normalize: uppercase, trim
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    while (!upper.empty() && upper.front() == ' ') upper.erase(upper.begin());
    while (!upper.empty() && upper.back() == ' ') upper.pop_back();

    if (upper == "SEND_CHECKPOINTS_TO_PEER")  return NinaActionType::SEND_CHECKPOINTS_TO_PEER;
    if (upper == "REQUEST_CHECKPOINTS")        return NinaActionType::REQUEST_CHECKPOINTS;
    if (upper == "GENERATE_CHECKPOINTS")       return NinaActionType::GENERATE_CHECKPOINTS;
    if (upper == "VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS") return NinaActionType::VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS;
    if (upper == "MONITOR_PEER")               return NinaActionType::MONITOR_PEER;
    if (upper == "INCREASE_PEER_TRUST")        return NinaActionType::INCREASE_PEER_TRUST;
    if (upper == "DECREASE_PEER_TRUST")        return NinaActionType::DECREASE_PEER_TRUST;
    if (upper == "QUARANTINE_PEER")            return NinaActionType::QUARANTINE_PEER;
    if (upper == "BROADCAST_STATE")            return NinaActionType::BROADCAST_STATE;
    if (upper == "ALERT_NETWORK")              return NinaActionType::ALERT_NETWORK;
    if (upper == "ADJUST_RING_THREAT")          return NinaActionType::ADJUST_RING_THREAT;
    if (upper == "PROTECT_NODE")                return NinaActionType::PROTECT_NODE;
    if (upper == "DIAGNOSE_NODE")               return NinaActionType::DIAGNOSE_NODE;
    if (upper == "RECOVER_NODE")                return NinaActionType::RECOVER_NODE;
    if (upper == "REINTEGRATE_NODE")            return NinaActionType::REINTEGRATE_NODE;
    if (upper == "ACTIVATE_COUNTERMEASURES")    return NinaActionType::ACTIVATE_COUNTERMEASURES;
    if (upper == "BLOCK_SPY_RELAY")             return NinaActionType::BLOCK_SPY_RELAY;
    if (upper == "CLOAK_PEER_LIST")             return NinaActionType::CLOAK_PEER_LIST;
    if (upper == "PROPAGATE_BAN")               return NinaActionType::PROPAGATE_BAN;
    if (upper.find("RING_THREAT_NORMAL") != std::string::npos)   return NinaActionType::ADJUST_RING_THREAT;
    if (upper.find("RING_THREAT_ELEVATED") != std::string::npos) return NinaActionType::ADJUST_RING_THREAT;
    if (upper.find("RING_THREAT_HIGH") != std::string::npos)     return NinaActionType::ADJUST_RING_THREAT;
    if (upper.find("RING_THREAT_CRITICAL") != std::string::npos) return NinaActionType::ADJUST_RING_THREAT;
    if (upper == "NO_ACTION")                  return NinaActionType::NO_ACTION;

    return NinaActionType::MONITOR_PEER;  // Safe default for unknown action strings
}

} // namespace ninacatcoin_ai
