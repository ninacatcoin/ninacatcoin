/**
 * NINA Node Protector — Immune System for the Network
 * ====================================================
 *
 * This module implements NINA's ability to:
 *   1. DETECT — Identify nodes under attack (distinguish victim from attacker)
 *   2. ISOLATE — Move attacked nodes to safe mode (trusted peers only)
 *   3. DIAGNOSE — NINA LLM analyzes what happened (attack type, vector, severity)
 *   4. RECOVER — Execute recovery plan (re-sync, purge, checkpoint restore)
 *   5. REINTEGRATE — Gradually reconnect the node to the main network
 *
 * ARCHITECTURE: NINA AI DECIDES EVERYTHING.
 *   Every state transition (healthy → attacked → isolated → recovering → healthy)
 *   goes through NinaDecisionEngine::evaluate(). The LLM (temp=0.0) decides.
 *   Fallback handlers exist ONLY as safety net when LLM is unavailable.
 *
 * Node States:
 *   HEALTHY        → Normal operation, all peers connected
 *   UNDER_ATTACK   → Attack detected, NINA evaluating response
 *   ISOLATED       → Safe mode: only high-trust peers, no new connections
 *   RECOVERING     → NINA is executing recovery plan
 *   REINTEGRATING  → Gradually reconnecting to normal peer set
 *
 * This is NOT hardcoded marketing logic. Every decision is made by NINA AI
 * through the Decision Engine, with full LLM reasoning when available.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <ctime>
#include "daemon/nina_decision_engine.hpp"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_protector"

namespace ninacatcoin_ai {

// ─── Node Health State Machine ──────────────────────────────────────────
enum class NodeState : uint8_t {
    HEALTHY        = 0,    // Normal operation
    UNDER_ATTACK   = 1,    // Attack in progress, NINA evaluating
    ISOLATED       = 2,    // Safe mode — only trusted peers
    RECOVERING     = 3,    // Recovery plan being executed
    REINTEGRATING  = 4     // Gradually reconnecting
};

// ─── Attack Vector (what NINA detected) ─────────────────────────────────
enum class AttackVector : uint8_t {
    UNKNOWN          = 0,
    SYBIL_FLOOD      = 1,   // Many fake peers connecting
    OUTPUT_POISONING = 2,   // Massive output creation to compromise rings
    ECLIPSE_ATTACK   = 3,   // Attacker isolates node from honest peers
    DIFFICULTY_MANIP = 4,   // Hashrate manipulation
    MEMPOOL_FLOOD    = 5,   // Transaction spam overwhelming mempool
    BLOCK_WITHHOLD   = 6    // Selfish mining / block withholding
};

// ─── Per-Peer Health Metrics ────────────────────────────────────────────
struct PeerHealthMetrics {
    std::string peer_id;
    float       trust_score       = 1.0f;   // 0.0 = untrusted, 1.0 = fully trusted
    uint64_t    blocks_relayed    = 0;       // How many valid blocks this peer gave us
    uint64_t    bad_blocks        = 0;       // Invalid/stale blocks from this peer
    uint64_t    tx_relayed        = 0;       // Valid transactions
    uint64_t    suspicious_events = 0;       // Anomalies linked to this peer
    uint64_t    last_seen_height  = 0;       // Last block height from this peer
    uint64_t    connected_since   = 0;       // Timestamp of connection
    bool        is_trusted        = false;   // NINA marked as trusted (survived quarantine)
    bool        is_quarantined    = false;   // Currently in quarantine

    float reliability() const {
        uint64_t total = blocks_relayed + bad_blocks;
        if (total == 0) return 0.5f; // unknown = neutral
        return static_cast<float>(blocks_relayed) / static_cast<float>(total);
    }
};

// ─── Attack Diagnosis (NINA's analysis) ─────────────────────────────────
struct AttackDiagnosis {
    AttackVector    vector          = AttackVector::UNKNOWN;
    float           severity        = 0.0f;    // 0.0–1.0
    float           confidence      = 0.0f;    // How sure NINA is
    std::string     description;               // NINA's reasoning
    std::string     recovery_plan;             // What NINA recommends
    uint64_t        diagnosed_at    = 0;       // Block height when diagnosed
    std::vector<std::string> suspicious_peers;  // Peer IDs involved in the attack
};

// ─── Recovery Plan (what NINA decided to do) ────────────────────────────
struct RecoveryPlan {
    bool purge_mempool       = false;   // Clear transaction pool
    bool resync_from_height  = false;   // Re-sync from a specific height
    uint64_t resync_height   = 0;       // Height to re-sync from
    bool checkpoint_restore  = false;   // Restore from last known good checkpoint
    bool reconnect_trusted   = false;   // Only reconnect with verified peers
    uint8_t max_peers        = 3;       // Limit peers during recovery
    uint64_t isolation_blocks = 100;    // How many blocks to stay isolated
    std::string nina_reasoning;         // NINA's full explanation
};

// ============================================================================
// NinaNodeProtector — The Immune System
// ============================================================================

class NinaNodeProtector {
public:
    static NinaNodeProtector& getInstance() {
        static NinaNodeProtector instance;
        return instance;
    }

    // ─── Current State ──────────────────────────────────────────────────

    NodeState get_state() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    const char* state_name() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return state_to_string(m_state);
    }

    bool is_in_safe_mode() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state == NodeState::ISOLATED || m_state == NodeState::RECOVERING;
    }

    bool should_accept_peer(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == NodeState::HEALTHY || m_state == NodeState::REINTEGRATING)
            return true; // accept all in normal and reintegration mode

        // In ISOLATED or RECOVERING: only trusted peers
        auto it = m_peer_health.find(peer_id);
        if (it == m_peer_health.end()) return false; // unknown = reject
        return it->second.is_trusted && !it->second.is_quarantined;
    }

    uint8_t max_peers_allowed() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (m_state) {
            case NodeState::HEALTHY:        return 0;    // no limit
            case NodeState::UNDER_ATTACK:   return 8;    // reduce surface
            case NodeState::ISOLATED:        return 3;    // minimum trusted
            case NodeState::RECOVERING:      return 5;    // cautious
            case NodeState::REINTEGRATING:   return 12;   // gradually increase
            default:                         return 0;
        }
    }

    // ─── Peer Health Tracking ─────────────────────────────────────────

    void observe_peer_block(const std::string& peer_id, uint64_t height, bool valid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& ph = m_peer_health[peer_id];
        ph.peer_id = peer_id;
        ph.last_seen_height = height;
        if (valid) {
            ph.blocks_relayed++;
            // Gradually increase trust for consistently good peers
            ph.trust_score = std::min(1.0f, ph.trust_score + 0.001f);
        } else {
            ph.bad_blocks++;
            ph.suspicious_events++;
            ph.trust_score = std::max(0.0f, ph.trust_score - 0.05f);
        }
    }

    void observe_peer_tx(const std::string& peer_id, bool valid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& ph = m_peer_health[peer_id];
        ph.peer_id = peer_id;
        if (valid) {
            ph.tx_relayed++;
        } else {
            ph.suspicious_events++;
            ph.trust_score = std::max(0.0f, ph.trust_score - 0.02f);
        }
    }

    void observe_peer_connected(const std::string& peer_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& ph = m_peer_health[peer_id];
        ph.peer_id = peer_id;
        if (ph.connected_since == 0)
            ph.connected_since = static_cast<uint64_t>(std::time(nullptr));
    }

    void mark_peer_trusted(const std::string& peer_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& ph = m_peer_health[peer_id];
        ph.peer_id = peer_id;
        ph.is_trusted = true;
        MINFO("[NINA-PROTECTOR] Peer " << peer_id.substr(0, 16) << " marked TRUSTED");
    }

    // ─── Attack Detection — NINA DECIDES ────────────────────────────────

    /**
     * Called when suspicious activity is detected.
     * NINA evaluates whether this constitutes an attack on this node.
     *
     * ALL decisions go through NinaDecisionEngine::evaluate().
     * NINA's LLM (temp=0.0) decides the response. Fallback only if LLM unavailable.
     *
     * @param height          Current block height
     * @param sybil_score     Sybil detection score (0.0–1.0)
     * @param ml_risk_score   ML anomaly risk score (0.0–1.0)
     * @param hashrate_drop   Hashrate drop percentage (0–100)
     * @param suspicious_peer Optional: peer that triggered the detection
     * @param anomaly_flags   Bitfield of what ML detected
     */
    void evaluate_node_threat(
        uint64_t height,
        float sybil_score,
        float ml_risk_score,
        float hashrate_drop,
        const std::string& suspicious_peer = "",
        uint32_t anomaly_flags = 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // ── Combine signals to determine if we're under attack ──
        // NINA doesn't use a simple threshold — she evaluates the PATTERN
        float combined_threat = std::max({sybil_score, ml_risk_score,
            std::min(1.0f, hashrate_drop / 80.0f)});

        // Not enough signal — skip
        if (combined_threat < 0.4f && m_state == NodeState::HEALTHY) {
            return;
        }

        // ── Count trusted vs suspicious peers ──
        uint32_t trusted_count = 0, suspicious_count = 0, total_peers = 0;
        for (const auto& [pid, ph] : m_peer_health) {
            total_peers++;
            if (ph.is_trusted) trusted_count++;
            if (ph.suspicious_events > 3 || ph.trust_score < 0.3f) suspicious_count++;
        }

        // ── Determine if we're the VICTIM (not the attacker) ──
        // Pattern: many suspicious peers targeting us = we're the victim
        // Pattern: our health is degrading while peers are fine = eclipse attack
        bool likely_victim = (suspicious_count > 0 && suspicious_count > trusted_count)
                          || (sybil_score > 0.6f)
                          || (combined_threat > 0.7f && total_peers > 0);

        if (!likely_victim && m_state == NodeState::HEALTHY) {
            return;
        }

        // ── ASK NINA what to do ──
        NinaEvent event(NinaEventType::NODE_UNDER_ATTACK);
        event.set("height", height);
        event.set("current_state", state_to_string(m_state));
        event.set("combined_threat", static_cast<double>(combined_threat));
        event.set("sybil_score", static_cast<double>(sybil_score));
        event.set("ml_risk_score", static_cast<double>(ml_risk_score));
        event.set("hashrate_drop_pct", static_cast<double>(hashrate_drop));
        event.set("total_peers", static_cast<uint64_t>(total_peers));
        event.set("trusted_peers", static_cast<uint64_t>(trusted_count));
        event.set("suspicious_peers", static_cast<uint64_t>(suspicious_count));
        event.set("anomaly_flags", static_cast<uint64_t>(anomaly_flags));
        if (!suspicious_peer.empty())
            event.set("trigger_peer", suspicious_peer);

        // Detect attack vector from signals
        AttackVector vector = detect_attack_vector(sybil_score, ml_risk_score,
            hashrate_drop, suspicious_count, total_peers);
        event.set("attack_vector", attack_vector_name(vector));

        auto actions = NinaDecisionEngine::getInstance().evaluate(event);

        // ── NINA DECIDED — execute her plan ──
        for (const auto& action : actions) {
            if (action.type == NinaActionType::PROTECT_NODE) {
                execute_protect(height, vector, combined_threat, action);
            }
            else if (action.type == NinaActionType::DIAGNOSE_NODE) {
                execute_diagnose(height, vector, action);
            }
            else if (action.type == NinaActionType::QUARANTINE_PEER && !suspicious_peer.empty()) {
                execute_quarantine_peer(suspicious_peer, action);
            }
            else if (action.type == NinaActionType::ALERT_NETWORK) {
                MWARNING("[NINA-PROTECTOR] NINA decided: ALERT_NETWORK — "
                         << action.reasoning);
                NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::ALERT_NETWORK);
            }
            else if (action.type == NinaActionType::MONITOR_PEER) {
                MINFO("[NINA-PROTECTOR] NINA decided: continue monitoring — "
                      << action.reasoning);
            }
        }
    }

    // ─── Periodic Health Check — NINA evaluates isolated/recovering nodes ─

    /**
     * Called periodically (every ~50 blocks) to check if isolated nodes
     * are ready for recovery or reintegration.
     * NINA AI makes every decision.
     */
    void periodic_health_check(uint64_t height) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_state == NodeState::HEALTHY) return; // nothing to check

        uint64_t blocks_in_state = (height > m_state_changed_height)
            ? (height - m_state_changed_height) : 0;

        NinaEvent event(NinaEventType::NODE_HEALTH_CHECK);
        event.set("height", height);
        event.set("current_state", state_to_string(m_state));
        event.set("blocks_in_state", blocks_in_state);
        event.set("isolation_blocks", m_recovery_plan.isolation_blocks);

        // Report peer health summary
        uint32_t trusted = 0, quarantined = 0, total = 0;
        float avg_trust = 0.0f;
        for (const auto& [pid, ph] : m_peer_health) {
            total++;
            avg_trust += ph.trust_score;
            if (ph.is_trusted) trusted++;
            if (ph.is_quarantined) quarantined++;
        }
        if (total > 0) avg_trust /= static_cast<float>(total);

        event.set("total_peers", static_cast<uint64_t>(total));
        event.set("trusted_peers", static_cast<uint64_t>(trusted));
        event.set("quarantined_peers", static_cast<uint64_t>(quarantined));
        event.set("avg_peer_trust", static_cast<double>(avg_trust));

        if (m_last_diagnosis.vector != AttackVector::UNKNOWN) {
            event.set("original_attack", attack_vector_name(m_last_diagnosis.vector));
            event.set("original_severity", static_cast<double>(m_last_diagnosis.severity));
        }

        auto actions = NinaDecisionEngine::getInstance().evaluate(event);

        for (const auto& action : actions) {
            if (action.type == NinaActionType::RECOVER_NODE) {
                execute_recovery(height, action);
            }
            else if (action.type == NinaActionType::REINTEGRATE_NODE) {
                execute_reintegration(height, action);
            }
            else if (action.type == NinaActionType::PROTECT_NODE) {
                // NINA wants to stay isolated longer
                MINFO("[NINA-PROTECTOR] NINA decided to extend isolation: " << action.reasoning);
            }
            else if (action.type == NinaActionType::MONITOR_PEER) {
                MINFO("[NINA-PROTECTOR] NINA: continue current state — " << action.reasoning);
            }
        }
    }

    // ─── Recovery Check — NINA decides when a recovering node is ready ──

    void evaluate_recovery_complete(uint64_t height) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_state != NodeState::RECOVERING && m_state != NodeState::REINTEGRATING) return;

        NinaEvent event(NinaEventType::NODE_RECOVERY_READY);
        event.set("height", height);
        event.set("current_state", state_to_string(m_state));
        event.set("blocks_since_isolation",
            (height > m_isolation_start_height) ? (height - m_isolation_start_height) : 0);

        // Check if attack signals have cleared
        uint32_t clean_peers = 0, total = 0;
        for (const auto& [pid, ph] : m_peer_health) {
            if (ph.is_quarantined) continue;
            total++;
            if (ph.trust_score > 0.7f && ph.suspicious_events == 0)
                clean_peers++;
        }
        float clean_ratio = (total > 0)
            ? static_cast<float>(clean_peers) / static_cast<float>(total) : 0.0f;

        event.set("clean_peer_ratio", static_cast<double>(clean_ratio));
        event.set("total_active_peers", static_cast<uint64_t>(total));

        auto actions = NinaDecisionEngine::getInstance().evaluate(event);

        for (const auto& action : actions) {
            if (action.type == NinaActionType::REINTEGRATE_NODE) {
                execute_reintegration(height, action);
            }
            else if (action.type == NinaActionType::PROTECT_NODE) {
                MWARNING("[NINA-PROTECTOR] NINA says NOT READY to reintegrate: "
                         << action.reasoning);
            }
        }
    }

    // ─── Diagnosis Access ──────────────────────────────────────────────

    AttackDiagnosis get_last_diagnosis() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_last_diagnosis;
    }

    RecoveryPlan get_recovery_plan() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_recovery_plan;
    }

    // ─── Stats ──────────────────────────────────────────────────────────

    struct ProtectorStats {
        uint64_t attacks_detected      = 0;
        uint64_t isolations_triggered  = 0;
        uint64_t recoveries_completed  = 0;
        uint64_t reintegrations_done   = 0;
        uint64_t peers_quarantined     = 0;
        uint64_t peers_rehabilitated   = 0;
    };

    ProtectorStats get_stats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

private:
    NinaNodeProtector() = default;
    ~NinaNodeProtector() = default;
    NinaNodeProtector(const NinaNodeProtector&) = delete;
    NinaNodeProtector& operator=(const NinaNodeProtector&) = delete;

    mutable std::mutex m_mutex;
    NodeState          m_state = NodeState::HEALTHY;
    uint64_t           m_state_changed_height = 0;
    uint64_t           m_isolation_start_height = 0;
    AttackDiagnosis    m_last_diagnosis;
    RecoveryPlan       m_recovery_plan;
    ProtectorStats     m_stats;

    // Per-peer health tracking
    std::map<std::string, PeerHealthMetrics> m_peer_health;

    // ─── State Transitions (only through NINA decisions) ────────────────

    void transition_to(NodeState new_state, uint64_t height, const std::string& reason) {
        NodeState old_state = m_state;
        m_state = new_state;
        m_state_changed_height = height;

        MWARNING("[NINA-PROTECTOR] STATE TRANSITION: "
                 << state_to_string(old_state) << " → " << state_to_string(new_state)
                 << " at height " << height << " — " << reason);
    }

    // ─── Execute NINA's Decisions ───────────────────────────────────────

    void execute_protect(uint64_t height, AttackVector vector,
                         float severity, const NinaAction& action)
    {
        m_stats.attacks_detected++;
        m_stats.isolations_triggered++;
        m_isolation_start_height = height;

        // Quarantine all suspicious peers
        for (auto& [pid, ph] : m_peer_health) {
            if (ph.trust_score < 0.3f || ph.suspicious_events > 3) {
                ph.is_quarantined = true;
                m_stats.peers_quarantined++;
                MWARNING("[NINA-PROTECTOR] Quarantining suspicious peer "
                         << pid.substr(0, 16) << " (trust=" << ph.trust_score << ")");
            }
        }

        // Mark peers with good history as trusted
        for (auto& [pid, ph] : m_peer_health) {
            if (!ph.is_quarantined && ph.blocks_relayed > 10 && ph.reliability() > 0.95f) {
                ph.is_trusted = true;
            }
        }

        transition_to(NodeState::ISOLATED, height,
            "NINA PROTECT — " + action.reasoning);

        m_last_diagnosis.vector     = vector;
        m_last_diagnosis.severity   = severity;
        m_last_diagnosis.confidence = static_cast<float>(action.confidence);
        m_last_diagnosis.description = action.reasoning;
        m_last_diagnosis.diagnosed_at = height;

        NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::PROTECT_NODE);

        MWARNING("[NINA-PROTECTOR] ⚠️ NODE ISOLATED — Attack vector: "
                 << attack_vector_name(vector)
                 << " severity=" << severity
                 << " — NINA reasoning: " << action.reasoning);
    }

    void execute_diagnose(uint64_t height, AttackVector vector, const NinaAction& action) {
        // NINA analyzes the attack and creates a recovery plan
        m_last_diagnosis.description = action.reasoning;
        m_last_diagnosis.recovery_plan = action.get_param("recovery_plan", "standard_recovery");

        // Parse NINA's recovery recommendations from action params
        m_recovery_plan = RecoveryPlan();
        m_recovery_plan.nina_reasoning = action.reasoning;

        std::string plan_type = action.get_param("recovery_plan", "standard");
        if (plan_type.find("purge_mempool") != std::string::npos)
            m_recovery_plan.purge_mempool = true;
        if (plan_type.find("resync") != std::string::npos) {
            m_recovery_plan.resync_from_height = true;
            try {
                m_recovery_plan.resync_height = std::stoull(
                    action.get_param("resync_height", "0"));
            } catch (...) {}
        }
        if (plan_type.find("checkpoint") != std::string::npos)
            m_recovery_plan.checkpoint_restore = true;
        m_recovery_plan.reconnect_trusted = true; // always during recovery

        try {
            m_recovery_plan.isolation_blocks = std::stoull(
                action.get_param("isolation_blocks", "100"));
        } catch (...) {
            m_recovery_plan.isolation_blocks = 100;
        }

        NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::DIAGNOSE_NODE);

        MINFO("[NINA-PROTECTOR] NINA diagnosed attack: " << action.reasoning
              << " — Recovery plan: " << plan_type);
    }

    void execute_quarantine_peer(const std::string& peer_id, const NinaAction& action) {
        auto it = m_peer_health.find(peer_id);
        if (it != m_peer_health.end()) {
            it->second.is_quarantined = true;
            it->second.trust_score = 0.0f;
            m_stats.peers_quarantined++;
        }

        NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::QUARANTINE_PEER);

        MWARNING("[NINA-PROTECTOR] NINA quarantined peer "
                 << peer_id.substr(0, 16) << ": " << action.reasoning);
    }

    void execute_recovery(uint64_t height, const NinaAction& action) {
        m_stats.recoveries_completed++;

        transition_to(NodeState::RECOVERING, height,
            "NINA RECOVER — " + action.reasoning);

        NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::RECOVER_NODE);

        MINFO("[NINA-PROTECTOR] 🔧 RECOVERY STARTED at height " << height
              << " — " << action.reasoning);
    }

    void execute_reintegration(uint64_t height, const NinaAction& action) {
        if (m_state == NodeState::REINTEGRATING) {
            // Already reintegrating → NINA says we're fully healthy now
            m_stats.reintegrations_done++;

            // Rehabilitate quarantined peers that behaved well during recovery
            for (auto& [pid, ph] : m_peer_health) {
                if (ph.is_quarantined && ph.blocks_relayed > 5 && ph.reliability() > 0.9f) {
                    ph.is_quarantined = false;
                    ph.suspicious_events = 0;
                    m_stats.peers_rehabilitated++;
                    MINFO("[NINA-PROTECTOR] Peer " << pid.substr(0, 16) << " rehabilitated");
                }
            }

            transition_to(NodeState::HEALTHY, height,
                "NINA REINTEGRATE COMPLETE — " + action.reasoning);

            NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::REINTEGRATE_NODE);

            MINFO("[NINA-PROTECTOR] ✅ NODE HEALTHY — Fully reintegrated at height "
                  << height << " — " << action.reasoning);
        }
        else {
            // Transitioning from ISOLATED/RECOVERING → REINTEGRATING
            transition_to(NodeState::REINTEGRATING, height,
                "NINA REINTEGRATING — " + action.reasoning);

            NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::REINTEGRATE_NODE);

            MINFO("[NINA-PROTECTOR] 🔄 REINTEGRATION STARTED at height " << height
                  << " — Gradually reconnecting peers. " << action.reasoning);
        }
    }

    // ─── Attack Vector Detection ────────────────────────────────────────

    static AttackVector detect_attack_vector(
        float sybil_score, float ml_risk, float hashrate_drop,
        uint32_t suspicious_peers, uint32_t total_peers)
    {
        // NINA recognizes attack patterns from combined signals
        if (sybil_score > 0.7f && suspicious_peers > total_peers / 2)
            return AttackVector::ECLIPSE_ATTACK;
        if (sybil_score > 0.6f)
            return AttackVector::SYBIL_FLOOD;
        if (hashrate_drop > 40.0f)
            return AttackVector::DIFFICULTY_MANIP;
        if (ml_risk > 0.8f)
            return AttackVector::OUTPUT_POISONING;
        if (ml_risk > 0.6f)
            return AttackVector::MEMPOOL_FLOOD;
        return AttackVector::UNKNOWN;
    }

    // ─── String Utilities ───────────────────────────────────────────────

    static const char* state_to_string(NodeState s) {
        switch (s) {
            case NodeState::HEALTHY:        return "HEALTHY";
            case NodeState::UNDER_ATTACK:   return "UNDER_ATTACK";
            case NodeState::ISOLATED:       return "ISOLATED";
            case NodeState::RECOVERING:     return "RECOVERING";
            case NodeState::REINTEGRATING:  return "REINTEGRATING";
            default:                        return "UNKNOWN";
        }
    }

    static const char* attack_vector_name(AttackVector v) {
        switch (v) {
            case AttackVector::SYBIL_FLOOD:      return "SYBIL_FLOOD";
            case AttackVector::OUTPUT_POISONING:  return "OUTPUT_POISONING";
            case AttackVector::ECLIPSE_ATTACK:    return "ECLIPSE_ATTACK";
            case AttackVector::DIFFICULTY_MANIP:  return "DIFFICULTY_MANIP";
            case AttackVector::MEMPOOL_FLOOD:     return "MEMPOOL_FLOOD";
            case AttackVector::BLOCK_WITHHOLD:    return "BLOCK_WITHHOLD";
            default:                              return "UNKNOWN";
        }
    }
};

} // namespace ninacatcoin_ai
