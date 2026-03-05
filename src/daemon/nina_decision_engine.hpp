/**
 * NINA Decision Engine — Where NINA Makes REAL Decisions
 *
 * Copyright (c) 2026, The NinaCatCoin Project
 *
 * This is the brain of NINA. Instead of the daemon using if/then/else logic
 * to decide when to send checkpoints, ban peers, or protection actions,
 * NINA's LLM evaluates each network event and decides what to do.
 *
 * Architecture:
 *   NinaEvent → NinaDecisionEngine::evaluate() → vector<NinaAction>
 *   ActionDispatcher (in protocol handler) executes the actions via P2P
 *
 * The LLM uses temperature=0.0 for deterministic decisions — every node
 * running the same model with the same input data will make the SAME decision.
 * This is critical for consensus: NINA's decisions are reproducible.
 *
 * Events that go to the LLM (rare, important):
 *   - NEW_PEER_CONNECTED: A new node joins — should we send checkpoints?
 *   - SYNC_COMPLETED: We just finished sync — broadcast our state?
 *   - CHECKPOINT_MISMATCH: A peer's checkpoints don't match — attack?
 *   - ANOMALY_DETECTED: Network anomaly — escalate?
 *
 * Events handled by embedded ML (frequent, fast):
 *   - BLOCK_RECEIVED: Every block — scored by ML, LLM only if anomalous
 *   - PEER_MESSAGE: Routine P2P — ML scoring only
 *
 * Fallback: If LLM is unavailable, default safe actions are used.
 * NINA never does nothing — she always has a plan.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <sstream>
#include <ctime>

// Forward declaration — full include only in .cpp

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_decision"

namespace ninacatcoin_ai {

// ============================================================================
// Event Types — What happened in the network
// ============================================================================

enum class NinaEventType {
    // === P2P Peer Events ===
    NEW_PEER_CONNECTED,        // A new peer just completed handshake
    PEER_DISCONNECTED,         // A peer disconnected
    PEER_SYNC_COMPLETED,       // A peer just finished syncing with us

    // === Checkpoint Events ===
    CHECKPOINT_REQUEST,        // A peer explicitly requests checkpoint data
    CHECKPOINT_MISMATCH,       // Our checkpoints don't match a peer's
    CHECKPOINT_FILES_MISSING,  // We don't have local checkpoint files yet

    // === Sync Events ===
    OWN_SYNC_COMPLETED,        // WE just finished syncing with the network
    INITIAL_BLOCKCHAIN_DOWNLOAD, // Node is downloading blockchain from scratch (height ~0)

    // === Security Events ===
    ANOMALY_DETECTED,          // Network anomaly detected by ML scoring
    SYBIL_SUSPECTED,           // Sybil attack patterns detected
    MODEL_HASH_MISMATCH,       // Peer runs a different NINA model

    // === Ring Signature Events ===
    RING_THREAT_EVALUATION,    // Evaluate traffic pattern for ring sizing

    // === Node Protection Events (NINA immune system) ===
    NODE_UNDER_ATTACK,         // A node is being targeted by attackers
    NODE_HEALTH_CHECK,         // Periodic health check on isolated/recovering node
    NODE_RECOVERY_READY,       // An isolated node appears ready to rejoin

    // === Spy Node Events (active countermeasures) ===
    SPY_NODE_DETECTED,         // Spy infrastructure detected — evaluate countermeasures

    // === Periodic ===
    HEALTH_CHECK               // Periodic health evaluation
};

// ============================================================================
// Action Types — What NINA decides to do
// ============================================================================

enum class NinaActionType {
    // === Checkpoint Actions ===
    SEND_CHECKPOINTS_TO_PEER,  // Send full checkpoint files to a specific peer
    REQUEST_CHECKPOINTS,       // Request checkpoint files from a specific peer
    GENERATE_CHECKPOINTS,      // Regenerate our local checkpoint files
    VALIDATE_BLOCKCHAIN_AGAINST_CHECKPOINTS, // Validate entire blockchain against both checkpoint types

    // === Peer Management ===
    MONITOR_PEER,              // Just watch this peer, no action needed
    INCREASE_PEER_TRUST,       // Peer is behaving well
    DECREASE_PEER_TRUST,       // Peer is suspicious
    QUARANTINE_PEER,           // Isolate this peer temporarily

    // === Network Actions ===
    BROADCAST_STATE,           // Broadcast our NINA state to all peers
    ALERT_NETWORK,             // Send security alert to connected peers

    // === Ring Signature Actions ===
    ADJUST_RING_THREAT,        // Set ring threat level (params: threat_level=0-3)

    // === Node Protection Actions (NINA immune system) ===
    PROTECT_NODE,              // Put node in safe mode (only trusted peers)
    DIAGNOSE_NODE,             // NINA LLM analyzes what happened to the node
    RECOVER_NODE,              // Execute recovery plan (re-sync, purge mempool, etc.)
    REINTEGRATE_NODE,          // Gradually reconnect node to the network

    // === Spy Countermeasure Actions ===
    ACTIVATE_COUNTERMEASURES,  // Set countermeasure level (params: level=PASSIVE/CAUTIOUS/ACTIVE/AGGRESSIVE)
    BLOCK_SPY_RELAY,           // Block TX relay to high-confidence spy nodes
    CLOAK_PEER_LIST,           // Give empty peer lists to spy nodes
    PROPAGATE_BAN,             // Share spy node IDs with trusted peers

    // === No-Op ===
    NO_ACTION                  // Explicitly decided to do nothing (still a decision)
};

// ============================================================================
// NinaEvent — Input to the Decision Engine
// ============================================================================

struct NinaEvent {
    NinaEventType type;
    uint64_t timestamp;
    
    // Context data — objective facts only (no timestamps/IPs for determinism)
    std::map<std::string, std::string> data;
    
    // Convenience constructor
    NinaEvent(NinaEventType t) 
        : type(t)
        , timestamp(static_cast<uint64_t>(std::time(nullptr)))
    {}

    void set(const std::string& key, const std::string& value) {
        data[key] = value;
    }
    void set(const std::string& key, uint64_t value) {
        data[key] = std::to_string(value);
    }
    void set(const std::string& key, double value) {
        std::ostringstream oss;
        oss << value;
        data[key] = oss.str();
    }
    void set(const std::string& key, bool value) {
        data[key] = value ? "true" : "false";
    }

    std::string get(const std::string& key, const std::string& default_val = "") const {
        auto it = data.find(key);
        return it != data.end() ? it->second : default_val;
    }

    std::string describe() const {
        std::ostringstream oss;
        oss << "Event{type=" << static_cast<int>(type);
        for (const auto& [k, v] : data)
            oss << ", " << k << "=" << v;
        oss << "}";
        return oss.str();
    }
};

// ============================================================================
// NinaAction — Output of the Decision Engine
// ============================================================================

struct NinaAction {
    NinaActionType type;
    double confidence;          // 0.0–1.0, how sure NINA is about this action
    std::string reasoning;      // NINA's explanation (for logging)
    
    // Parameters for the dispatcher
    std::map<std::string, std::string> params;
    
    NinaAction(NinaActionType t, double conf, const std::string& reason)
        : type(t), confidence(conf), reasoning(reason)
    {}

    void set_param(const std::string& key, const std::string& value) {
        params[key] = value;
    }

    std::string get_param(const std::string& key, const std::string& default_val = "") const {
        auto it = params.find(key);
        return it != params.end() ? it->second : default_val;
    }

    std::string describe() const {
        std::ostringstream oss;
        oss << "Action{type=" << static_cast<int>(type) 
            << ", confidence=" << confidence
            << ", reason=" << reasoning << "}";
        return oss.str();
    }
};

// ============================================================================
// NinaDecisionEngine — The Brain
// ============================================================================

class NinaDecisionEngine {
public:
    static NinaDecisionEngine& getInstance() {
        static NinaDecisionEngine instance;
        return instance;
    }

    /**
     * @brief Evaluate a network event and decide what actions to take.
     * 
     * This is THE central method. Every significant network event goes through
     * here. NINA evaluates the situation using her LLM (temp=0.0 for
     * deterministic output) and returns a list of actions to execute.
     *
     * If the LLM is unavailable, safe fallback actions are returned.
     * NINA NEVER returns an empty list — she always has a plan.
     *
     * @param event The network event to evaluate
     * @return Vector of actions to execute (ordered by priority)
     */
    std::vector<NinaAction> evaluate(const NinaEvent& event);

    /**
     * @brief Get statistics about NINA's decision-making
     */
    struct DecisionStats {
        uint64_t total_events = 0;
        uint64_t llm_decisions = 0;
        uint64_t fallback_decisions = 0;
        uint64_t actions_generated = 0;
        uint64_t checkpoints_sent = 0;
        uint64_t checkpoints_requested = 0;
        uint64_t ring_decisions = 0;
        uint64_t anomaly_alerts = 0;
        uint64_t sybil_actions = 0;
        uint64_t nodes_protected = 0;
        uint64_t nodes_recovered = 0;
        uint64_t nodes_reintegrated = 0;
        uint64_t spy_countermeasures = 0;
    };
    DecisionStats get_stats() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats; 
    }

    void record_action_executed(NinaActionType type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.actions_generated++;
        if (type == NinaActionType::SEND_CHECKPOINTS_TO_PEER)
            m_stats.checkpoints_sent++;
        else if (type == NinaActionType::REQUEST_CHECKPOINTS)
            m_stats.checkpoints_requested++;
        else if (type == NinaActionType::ADJUST_RING_THREAT)
            m_stats.ring_decisions++;
        else if (type == NinaActionType::ALERT_NETWORK)
            m_stats.anomaly_alerts++;
        else if (type == NinaActionType::QUARANTINE_PEER || type == NinaActionType::DECREASE_PEER_TRUST)
            m_stats.sybil_actions++;
        else if (type == NinaActionType::PROTECT_NODE)
            m_stats.nodes_protected++;
        else if (type == NinaActionType::RECOVER_NODE)
            m_stats.nodes_recovered++;
        else if (type == NinaActionType::REINTEGRATE_NODE)
            m_stats.nodes_reintegrated++;
        else if (type == NinaActionType::ACTIVATE_COUNTERMEASURES
              || type == NinaActionType::BLOCK_SPY_RELAY
              || type == NinaActionType::CLOAK_PEER_LIST
              || type == NinaActionType::PROPAGATE_BAN)
            m_stats.spy_countermeasures++;
    }

private:
    NinaDecisionEngine() = default;
    ~NinaDecisionEngine() = default;
    NinaDecisionEngine(const NinaDecisionEngine&) = delete;
    NinaDecisionEngine& operator=(const NinaDecisionEngine&) = delete;

    /**
     * @brief Ask the LLM to evaluate the event
     * Uses temperature=0.0 for deterministic decisions.
     * Returns parsed actions from the LLM's structured output.
     */
    std::vector<NinaAction> evaluate_with_llm(const NinaEvent& event);

    /**
     * @brief Safe fallback decisions when LLM is unavailable
     * Uses conservative defaults that protect the network.
     */
    std::vector<NinaAction> evaluate_fallback(const NinaEvent& event);

    /**
     * @brief Build the decision prompt from event data
     * Only includes objective, deterministic data — no timestamps, no IPs.
     */
    std::string build_decision_prompt(const NinaEvent& event) const;

    /**
     * @brief Parse the LLM's structured output into NinaAction objects
     */
    std::vector<NinaAction> parse_decision_output(const std::string& raw_output, const NinaEvent& event) const;

    /**
     * @brief Convert event type to human-readable string for the prompt
     */
    static std::string event_type_to_string(NinaEventType type);

    /**
     * @brief Convert action string from LLM output to NinaActionType
     */
    static NinaActionType string_to_action_type(const std::string& s);

    mutable std::mutex m_mutex;
    DecisionStats m_stats;
};

} // namespace ninacatcoin_ai
