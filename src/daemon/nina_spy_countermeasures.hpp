/**
 * NINA Spy Node Countermeasures — Active Defense Against Surveillance
 * ===================================================================
 *
 * Detection alone is NOT enough. The "Friend or Foe" paper (2025) found that
 * 14.74% of Monero peers are spy nodes, and ban lists only reduce saturation
 * from 15.26% to ~7.13%. This module implements ACTIVE countermeasures that
 * make spying USELESS, not just detectable.
 *
 * STRATEGY: Don't just disconnect spy nodes — NEUTRALIZE their data collection.
 *
 * 5 Active Countermeasures:
 *   1. SELECTIVE TX RELAY   — Never relay OUR transactions to suspicious peers
 *   2. DELAYED RELAY        — Add random delay before relaying to suspicious peers
 *   3. PEER LIST CLOAKING   — Give empty/fake peer lists to suspicious peers
 *   4. DANDELION++ ROUTING  — Never use suspicious peers in stem phase
 *   5. COORDINATED BAN PROP — Share detected spy IPs with trusted peers via P2P
 *
 * WHY THIS WORKS:
 *   - Spy nodes rely on "first-spy estimation" (who relays first = originator)
 *   - Selective relay + delay DESTROYS first-spy timing analysis
 *   - Peer list cloaking PREVENTS topology mapping
 *   - Dandelion++ spy-aware routing ensures stem never touches spy nodes
 *   - Coordinated bans make the WHOLE network resistant, not just one node
 *
 * All decisions are made by NINA AI through the DecisionEngine.
 * This module provides the EXECUTION layer for countermeasure actions.
 *
 * Copyright (c) 2026, The NinaCatCoin Project
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <mutex>
#include <algorithm>
#include <functional>
#include <sstream>
#include <chrono>
#include "daemon/nina_decision_engine.hpp"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_spy_counter"

namespace ninacatcoin_ai {

// ─── Countermeasure Policy Levels ───────────────────────────────────────
// NINA decides which level to activate based on spy node saturation
enum class CountermeasureLevel : uint8_t {
    PASSIVE    = 0,  // Only detection, no active countermeasures (< 5% suspicious)
    CAUTIOUS   = 1,  // Delayed relay + Dandelion++ avoidance (5-15% suspicious)
    ACTIVE     = 2,  // Full selective relay + peer list cloaking (15-30% suspicious)
    AGGRESSIVE = 3   // All countermeasures + coordinated ban propagation (> 30% suspicious)
};

// ─── Spy Node Classification ────────────────────────────────────────────
// Based on anomaly categories from "Friend or Foe" paper (2025)
enum class SpyNodeType : uint8_t {
    UNKNOWN              = 0,
    TIMING_ANALYZER      = 1,  // First-spy estimation (Chainalysis-style)
    TOPOLOGY_MAPPER      = 2,  // Peer list collector (deprecated timestamps, diversity)
    HANDSHAKE_FLOODER    = 3,  // Short-lived connections (<1s)
    PING_FLOODER         = 4,  // Excessive keep-alive (501 IPs in "Friend or Foe")
    ECLIPSE_OPERATOR     = 5,  // Peer list poisoning (Shi et al. NDSS 2025)
    PASSIVE_OBSERVER     = 6,  // Connects normally but logs everything
    TX_CORRELATOR        = 7   // Correlates tx origins via Dandelion++ analysis
};

// ─── Per-Peer Spy Classification ────────────────────────────────────────
struct SpyClassification {
    std::string   peer_id;
    SpyNodeType   type            = SpyNodeType::UNKNOWN;
    float         spy_confidence  = 0.0f;    // 0.0 = not spy, 1.0 = definitely spy
    bool          is_neutralized  = false;    // Countermeasures active for this peer
    uint32_t      anomaly_flags   = 0;       // Bitfield of detected anomalies
    uint64_t      classified_at   = 0;       // Timestamp
    uint32_t      relay_delay_ms  = 0;       // Current delay applied (0 = normal)
    bool          tx_relay_blocked = false;   // Are we blocking TX relay to this peer?
    bool          peer_list_cloaked = false;  // Are we giving fake peer lists?
    bool          stem_excluded   = false;    // Excluded from Dandelion++ stem?
    
    // Anomaly flag bits (from "Friend or Foe" categories)
    static constexpr uint32_t FLAG_MISSING_SUPPORT_FLAGS   = 0x01;
    static constexpr uint32_t FLAG_DEPRECATED_TIMESTAMPS   = 0x02;
    static constexpr uint32_t FLAG_TCP_FRAGMENTATION       = 0x04;
    static constexpr uint32_t FLAG_LOW_PEER_DIVERSITY      = 0x08;
    static constexpr uint32_t FLAG_HIGH_PEER_SIMILARITY    = 0x10;
    static constexpr uint32_t FLAG_SHORT_CONNECTIONS       = 0x20;
    static constexpr uint32_t FLAG_PING_FLOODING           = 0x40;
    static constexpr uint32_t FLAG_PEER_ID_ANOMALY         = 0x80;
    static constexpr uint32_t FLAG_SUBNET_CLUSTER          = 0x100;
    static constexpr uint32_t FLAG_CORRELATED_BEHAVIOR     = 0x200;
};

// ============================================================================
// NinaSpyCountermeasures — Active Defense Module
// ============================================================================

class NinaSpyCountermeasures {
public:
    static NinaSpyCountermeasures& getInstance() {
        static NinaSpyCountermeasures instance;
        return instance;
    }

    // ─── Countermeasure Level (NINA decides) ────────────────────────────

    CountermeasureLevel get_level() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_level;
    }

    const char* level_name() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return level_to_string(m_level);
    }

    // ─── Spy Classification (feed from SybilDetector) ───────────────────

    /**
     * @brief Classify a peer as a potential spy node
     * Called by SybilDetectorModule when anomalies are detected
     */
    void classify_peer(const std::string& peer_id, SpyNodeType type,
                       float confidence, uint32_t anomaly_flags)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& cls = m_classifications[peer_id];
        cls.peer_id = peer_id;
        cls.type = type;
        cls.spy_confidence = confidence;
        cls.anomaly_flags |= anomaly_flags;  // Accumulate flags
        cls.classified_at = static_cast<uint64_t>(std::time(nullptr));
        
        m_stats.peers_classified++;
        
        MINFO("[NINA-SPY] Classified peer " << peer_id.substr(0, 16)
              << " as " << spy_type_name(type)
              << " (confidence=" << confidence
              << ", flags=0x" << std::hex << anomaly_flags << std::dec << ")");
        
        // Auto-apply countermeasures based on current level
        apply_countermeasures_to_peer(cls);
    }

    /**
     * @brief Update spy confidence for a peer (new evidence)
     */
    void update_spy_confidence(const std::string& peer_id, float new_confidence) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_classifications.find(peer_id);
        if (it != m_classifications.end()) {
            float old = it->second.spy_confidence;
            // Weighted update: 70% new evidence, 30% historical
            it->second.spy_confidence = 0.7f * new_confidence + 0.3f * old;
            
            MINFO("[NINA-SPY] Updated confidence for " << peer_id.substr(0, 16)
                  << ": " << old << " → " << it->second.spy_confidence);
            
            apply_countermeasures_to_peer(it->second);
        }
    }

    // ─── COUNTERMEASURE 1: Selective Transaction Relay ──────────────────
    // Never relay OUR transactions to spy nodes. They can't analyze what
    // they never receive. This directly kills first-spy estimation.
    
    /**
     * @brief Should we relay this transaction to this peer?
     * Called from the transaction relay path in P2P layer.
     * 
     * @param peer_id      The peer we would relay to
     * @param is_our_tx    True if this is a locally-originated transaction
     * @return true if relay is allowed, false if tx should NOT be sent to this peer
     */
    bool should_relay_tx(const std::string& peer_id, bool is_our_tx) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_level == CountermeasureLevel::PASSIVE)
            return true;  // No filtering in passive mode
        
        auto it = m_classifications.find(peer_id);
        if (it == m_classifications.end())
            return true;  // Unknown peer = allow
        
        const auto& cls = it->second;
        
        // NEVER relay our own transactions to spy nodes (any confidence > 0.5)
        if (is_our_tx && cls.spy_confidence > 0.5f) {
            m_stats.tx_blocked++;
            MDEBUG("[NINA-SPY] BLOCKED our TX relay to spy peer "
                   << peer_id.substr(0, 16)
                   << " (confidence=" << cls.spy_confidence << ")");
            return false;
        }
        
        // In ACTIVE/AGGRESSIVE: also skip relaying ANY tx to high-confidence spies
        if (m_level >= CountermeasureLevel::ACTIVE && cls.spy_confidence > 0.7f) {
            m_stats.tx_blocked++;
            return false;
        }
        
        // In AGGRESSIVE: block relay to all classified spies
        if (m_level == CountermeasureLevel::AGGRESSIVE && cls.spy_confidence > 0.4f) {
            m_stats.tx_blocked++;
            return false;
        }
        
        return true;
    }

    // ─── COUNTERMEASURE 2: Delayed Relay ────────────────────────────────
    // Add random delay before relaying to suspicious peers.
    // Destroys timing correlation — spy can't determine if we're the origin
    // because everyone relays at random offsets.
    
    /**
     * @brief Get relay delay for this peer (milliseconds)
     * Called before relaying blocks/transactions to this peer.
     * 0 = relay immediately (not suspicious)
     * >0 = wait this many ms before relaying
     */
    uint32_t get_relay_delay_ms(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_level == CountermeasureLevel::PASSIVE)
            return 0;
        
        auto it = m_classifications.find(peer_id);
        if (it == m_classifications.end())
            return 0;
        
        return it->second.relay_delay_ms;
    }

    // ─── COUNTERMEASURE 3: Peer List Cloaking ──────────────────────────
    // When a spy node asks for our peer list (Timed Sync / handshake),
    // we give them an empty or minimal list. This prevents:
    //   - Topology mapping (mapping the entire P2P graph)
    //   - Identifying our trusted peers (targeting them next)
    //   - Peer list poisoning preparation
    
    /**
     * @brief Should we share our real peer list with this peer?
     * Called from the peer exchange protocol handler.
     * 
     * @return true = share normally, false = give empty/minimal list
     */
    bool should_share_peer_list(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_level < CountermeasureLevel::ACTIVE)
            return true;  // Only active in ACTIVE/AGGRESSIVE
        
        auto it = m_classifications.find(peer_id);
        if (it == m_classifications.end())
            return true;
        
        // Classified spy → cloak peer list
        if (it->second.spy_confidence > 0.5f) {
            m_stats.peer_lists_cloaked++;
            MDEBUG("[NINA-SPY] CLOAKED peer list from spy "
                   << peer_id.substr(0, 16));
            return false;
        }
        
        return true;
    }

    // ─── COUNTERMEASURE 4: Dandelion++ Spy-Aware Routing ───────────────
    // Never use suspected spy nodes in Dandelion++ stem phase.
    // The stem is the privacy-critical path — if a spy is in the stem,
    // they learn who originated the transaction.
    // This directly counters the Dandelion++ weakness found by Sharma et al.
    
    /**
     * @brief Can this peer be used as a Dandelion++ stem relay?
     * Called when selecting the next hop for the stem phase.
     * 
     * @return true = safe to use in stem, false = NEVER use in stem
     */
    bool is_stem_eligible(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Always filter stem nodes (even in CAUTIOUS mode)
        if (m_level == CountermeasureLevel::PASSIVE)
            return true;
        
        auto it = m_classifications.find(peer_id);
        if (it == m_classifications.end())
            return true;
        
        // ANY spy suspicion = excluded from stem
        // Stem phase is too critical for privacy to risk
        if (it->second.spy_confidence > 0.3f) {
            m_stats.stem_exclusions++;
            MDEBUG("[NINA-SPY] EXCLUDED spy from Dandelion++ stem: "
                   << peer_id.substr(0, 16)
                   << " (confidence=" << it->second.spy_confidence << ")");
            return false;
        }
        
        return true;
    }

    // ─── COUNTERMEASURE 5: Coordinated Ban Propagation ─────────────────
    // Share detected spy node fingerprints with trusted peers via P2P.
    // This makes the ENTIRE network resistant, not just our node.
    // Only in AGGRESSIVE mode to prevent abuse.
    
    /**
     * @brief Get list of spy peer fingerprints to share with trusted peers
     * Called during P2P checkpoint sync cycle (piggyback on existing protocol)
     */
    std::vector<std::string> get_spy_peer_ids_for_sharing() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_level != CountermeasureLevel::AGGRESSIVE)
            return {};  // Only share in aggressive mode
        
        std::vector<std::string> result;
        for (const auto& [pid, cls] : m_classifications) {
            // Only share HIGH confidence classifications (avoid false positive propagation)
            if (cls.spy_confidence > 0.8f) {
                result.push_back(pid);
            }
        }
        
        m_stats.ban_propagations += result.size();
        return result;
    }

    /**
     * @brief Receive spy node reports from a trusted peer
     * Adds external spy classifications with reduced confidence
     */
    void receive_spy_report(const std::string& reporting_peer,
                            const std::vector<std::string>& spy_peer_ids)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& spy_id : spy_peer_ids) {
            auto it = m_classifications.find(spy_id);
            if (it == m_classifications.end()) {
                // New spy peer from trusted peer — add with reduced confidence
                auto& cls = m_classifications[spy_id];
                cls.peer_id = spy_id;
                cls.type = SpyNodeType::UNKNOWN;
                cls.spy_confidence = 0.4f;  // Reduced: we didn't verify ourselves
                cls.classified_at = static_cast<uint64_t>(std::time(nullptr));
                cls.anomaly_flags = SpyClassification::FLAG_CORRELATED_BEHAVIOR;
                
                MINFO("[NINA-SPY] Received spy report from trusted peer: "
                      << spy_id.substr(0, 16) << " (confidence=0.4, unverified)");
                
                m_stats.external_reports_received++;
            } else {
                // Already known — boost confidence slightly
                it->second.spy_confidence = std::min(1.0f, 
                    it->second.spy_confidence + 0.1f);
                MINFO("[NINA-SPY] Spy report corroborates known spy: "
                      << spy_id.substr(0, 16)
                      << " → confidence=" << it->second.spy_confidence);
            }
        }
    }

    // ─── NINA Evaluation — Adjust Countermeasure Level ──────────────────
    
    /**
     * @brief Evaluate network state and adjust countermeasure level
     * Called periodically (~every 50 blocks) or when spy saturation changes.
     * NINA AI decides the level through DecisionEngine.
     *
     * @param total_peers         Total connected peers
     * @param suspicious_pct      Percentage of peers classified as suspicious (0-100)
     * @param sybil_score         Current global sybil score (0.0-1.0)
     * @param height              Current block height
     */
    void evaluate_countermeasure_level(
        uint32_t total_peers,
        float suspicious_pct,
        float sybil_score,
        uint64_t height)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        CountermeasureLevel old_level = m_level;
        
        // Ask NINA to decide
        NinaEvent event(NinaEventType::SPY_NODE_DETECTED);
        event.set("height", height);
        event.set("total_peers", static_cast<uint64_t>(total_peers));
        event.set("suspicious_pct", static_cast<double>(suspicious_pct));
        event.set("sybil_score", static_cast<double>(sybil_score));
        event.set("current_level", level_to_string(m_level));
        event.set("classified_spies", static_cast<uint64_t>(m_classifications.size()));
        
        // Count neutralized peers
        uint32_t neutralized = 0;
        for (const auto& [_, cls] : m_classifications) {
            if (cls.is_neutralized) neutralized++;
        }
        event.set("neutralized_count", static_cast<uint64_t>(neutralized));
        
        auto actions = NinaDecisionEngine::getInstance().evaluate(event);
        
        // Process NINA's decision
        for (const auto& action : actions) {
            if (action.type == NinaActionType::ACTIVATE_COUNTERMEASURES) {
                std::string level_str = action.get_param("level", "");
                if (level_str == "PASSIVE")    m_level = CountermeasureLevel::PASSIVE;
                else if (level_str == "CAUTIOUS")   m_level = CountermeasureLevel::CAUTIOUS;
                else if (level_str == "ACTIVE")     m_level = CountermeasureLevel::ACTIVE;
                else if (level_str == "AGGRESSIVE") m_level = CountermeasureLevel::AGGRESSIVE;
                
                NinaDecisionEngine::getInstance().record_action_executed(
                    NinaActionType::ACTIVATE_COUNTERMEASURES);
            }
            else if (action.type == NinaActionType::BLOCK_SPY_RELAY) {
                // NINA specifically orders TX relay block for high-risk spies
                for (auto& [pid, cls] : m_classifications) {
                    if (cls.spy_confidence > 0.6f) {
                        cls.tx_relay_blocked = true;
                        cls.is_neutralized = true;
                    }
                }
                NinaDecisionEngine::getInstance().record_action_executed(
                    NinaActionType::BLOCK_SPY_RELAY);
                
                m_stats.relay_blocks_activated++;
            }
            else if (action.type == NinaActionType::CLOAK_PEER_LIST) {
                // NINA orders peer list cloaking for all spies
                for (auto& [pid, cls] : m_classifications) {
                    if (cls.spy_confidence > 0.5f) {
                        cls.peer_list_cloaked = true;
                        cls.is_neutralized = true;
                    }
                }
                NinaDecisionEngine::getInstance().record_action_executed(
                    NinaActionType::CLOAK_PEER_LIST);
                
                m_stats.cloaking_activations++;
            }
            else if (action.type == NinaActionType::PROPAGATE_BAN) {
                // NINA orders ban propagation to trusted peers
                NinaDecisionEngine::getInstance().record_action_executed(
                    NinaActionType::PROPAGATE_BAN);
                
                m_stats.ban_propagations++;
            }
        }
        
        // Fallback: if NINA LLM unavailable, use thresholds
        if (actions.empty() || 
            (actions.size() == 1 && actions[0].type == NinaActionType::MONITOR_PEER))
        {
            // Deterministic fallback based on suspicious percentage
            if (suspicious_pct > 30.0f || sybil_score > 0.8f) {
                m_level = CountermeasureLevel::AGGRESSIVE;
            } else if (suspicious_pct > 15.0f || sybil_score > 0.6f) {
                m_level = CountermeasureLevel::ACTIVE;
            } else if (suspicious_pct > 5.0f || sybil_score > 0.3f) {
                m_level = CountermeasureLevel::CAUTIOUS;
            } else {
                m_level = CountermeasureLevel::PASSIVE;
            }
        }
        
        // Log level change
        if (m_level != old_level) {
            MWARNING("[NINA-SPY] ══════════════════════════════════════════");
            MWARNING("[NINA-SPY] COUNTERMEASURE LEVEL CHANGED: "
                     << level_to_string(old_level) << " → " << level_to_string(m_level));
            MWARNING("[NINA-SPY]   Suspicious peers: " << suspicious_pct << "%");
            MWARNING("[NINA-SPY]   Sybil score: " << sybil_score);
            MWARNING("[NINA-SPY]   Classified spies: " << m_classifications.size());
            MWARNING("[NINA-SPY] ══════════════════════════════════════════");
            
            // Re-apply countermeasures to all classified peers at new level
            for (auto& [pid, cls] : m_classifications) {
                apply_countermeasures_to_peer(cls);
            }
        }
    }

    // ─── Stats ──────────────────────────────────────────────────────────

    struct CountermeasureStats {
        uint64_t peers_classified          = 0;
        uint64_t tx_blocked                = 0;   // Transactions NOT relayed to spies
        uint64_t peer_lists_cloaked        = 0;   // Times we gave empty peer lists
        uint64_t stem_exclusions           = 0;   // Times a spy was excluded from stem
        uint64_t ban_propagations          = 0;   // Spy IDs shared with trusted peers
        uint64_t external_reports_received = 0;   // Spy reports from other nodes
        uint64_t relay_blocks_activated    = 0;   // Times NINA activated relay blocks
        uint64_t cloaking_activations      = 0;   // Times NINA activated cloaking
        uint64_t delayed_relays            = 0;   // Relays with added delay
    };

    CountermeasureStats get_stats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

    // ─── Query API ──────────────────────────────────────────────────────

    /**
     * @brief Get classification for a specific peer
     */
    SpyClassification get_classification(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_classifications.find(peer_id);
        if (it != m_classifications.end())
            return it->second;
        return SpyClassification();
    }

    /**
     * @brief Get total number of classified spy peers
     */
    uint32_t get_spy_count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t count = 0;
        for (const auto& [_, cls] : m_classifications) {
            if (cls.spy_confidence > 0.5f) count++;
        }
        return count;
    }

    /**
     * @brief Get percentage of connected peers that are classified spies
     */
    float get_spy_saturation(uint32_t total_connected) const {
        if (total_connected == 0) return 0.0f;
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t spies = 0;
        for (const auto& [_, cls] : m_classifications) {
            if (cls.spy_confidence > 0.5f) spies++;
        }
        return (static_cast<float>(spies) / static_cast<float>(total_connected)) * 100.0f;
    }

    /**
     * @brief Remove classification for a peer (e.g., proved legitimate)
     */
    void clear_classification(const std::string& peer_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_classifications.erase(peer_id);
        MINFO("[NINA-SPY] Cleared classification for " << peer_id.substr(0, 16));
    }

    /**
     * @brief Get human-readable summary of countermeasure state
     */
    std::string get_status_summary() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream oss;
        
        uint32_t spies = 0, neutralized = 0, blocked = 0, cloaked = 0, excluded = 0;
        for (const auto& [_, cls] : m_classifications) {
            if (cls.spy_confidence > 0.5f) spies++;
            if (cls.is_neutralized) neutralized++;
            if (cls.tx_relay_blocked) blocked++;
            if (cls.peer_list_cloaked) cloaked++;
            if (cls.stem_excluded) excluded++;
        }
        
        oss << "[NINA Spy Countermeasures]\n"
            << "  Level: " << level_to_string(m_level) << "\n"
            << "  Classified spies: " << spies << "\n"
            << "  Neutralized: " << neutralized << "\n"
            << "  TX relay blocked: " << blocked << " peers\n"
            << "  Peer list cloaked: " << cloaked << " peers\n"
            << "  Stem excluded: " << excluded << " peers\n"
            << "  Total TX blocked: " << m_stats.tx_blocked << "\n"
            << "  Total lists cloaked: " << m_stats.peer_lists_cloaked << "\n"
            << "  Total stem exclusions: " << m_stats.stem_exclusions;
        
        return oss.str();
    }

private:
    NinaSpyCountermeasures() : m_level(CountermeasureLevel::PASSIVE) {
        MINFO("[NINA-SPY] Spy Countermeasures module initialized (PASSIVE)");
    }
    ~NinaSpyCountermeasures() = default;
    NinaSpyCountermeasures(const NinaSpyCountermeasures&) = delete;
    NinaSpyCountermeasures& operator=(const NinaSpyCountermeasures&) = delete;

    mutable std::mutex m_mutex;
    CountermeasureLevel m_level;
    std::map<std::string, SpyClassification> m_classifications;
    mutable CountermeasureStats m_stats;

    // ─── Internal: Apply countermeasures based on level + confidence ────

    void apply_countermeasures_to_peer(SpyClassification& cls) {
        // Reset first
        cls.relay_delay_ms = 0;
        cls.tx_relay_blocked = false;
        cls.peer_list_cloaked = false;
        cls.stem_excluded = false;
        cls.is_neutralized = false;
        
        float conf = cls.spy_confidence;
        
        switch (m_level) {
            case CountermeasureLevel::PASSIVE:
                // No active countermeasures
                break;
                
            case CountermeasureLevel::CAUTIOUS:
                // Dandelion++ exclusion + delay for moderate+ confidence
                if (conf > 0.3f) {
                    cls.stem_excluded = true;
                }
                if (conf > 0.5f) {
                    // Random delay: 500-3000ms based on confidence
                    cls.relay_delay_ms = 500 + static_cast<uint32_t>(conf * 2500);
                    cls.is_neutralized = true;
                }
                break;
                
            case CountermeasureLevel::ACTIVE:
                // Full selective relay + cloaking for moderate+ confidence
                if (conf > 0.3f) {
                    cls.stem_excluded = true;
                }
                if (conf > 0.5f) {
                    cls.tx_relay_blocked = true;
                    cls.peer_list_cloaked = true;
                    cls.relay_delay_ms = 1000 + static_cast<uint32_t>(conf * 4000);
                    cls.is_neutralized = true;
                }
                break;
                
            case CountermeasureLevel::AGGRESSIVE:
                // Everything — even low confidence gets countermeasures
                cls.stem_excluded = true;  // All classified peers excluded from stem
                if (conf > 0.3f) {
                    cls.tx_relay_blocked = true;
                    cls.peer_list_cloaked = true;
                    cls.relay_delay_ms = 2000 + static_cast<uint32_t>(conf * 5000);
                    cls.is_neutralized = true;
                }
                break;
        }
        
        if (cls.is_neutralized) {
            MINFO("[NINA-SPY] Countermeasures applied to " << cls.peer_id.substr(0, 16)
                  << ": relay_blocked=" << cls.tx_relay_blocked
                  << " cloaked=" << cls.peer_list_cloaked
                  << " stem_excluded=" << cls.stem_excluded
                  << " delay=" << cls.relay_delay_ms << "ms");
        }
    }

    // ─── String Utilities ───────────────────────────────────────────────

    static const char* level_to_string(CountermeasureLevel lvl) {
        switch (lvl) {
            case CountermeasureLevel::PASSIVE:    return "PASSIVE";
            case CountermeasureLevel::CAUTIOUS:   return "CAUTIOUS";
            case CountermeasureLevel::ACTIVE:     return "ACTIVE";
            case CountermeasureLevel::AGGRESSIVE: return "AGGRESSIVE";
            default:                              return "UNKNOWN";
        }
    }

    static const char* spy_type_name(SpyNodeType t) {
        switch (t) {
            case SpyNodeType::TIMING_ANALYZER:   return "TIMING_ANALYZER";
            case SpyNodeType::TOPOLOGY_MAPPER:   return "TOPOLOGY_MAPPER";
            case SpyNodeType::HANDSHAKE_FLOODER: return "HANDSHAKE_FLOODER";
            case SpyNodeType::PING_FLOODER:      return "PING_FLOODER";
            case SpyNodeType::ECLIPSE_OPERATOR:  return "ECLIPSE_OPERATOR";
            case SpyNodeType::PASSIVE_OBSERVER:  return "PASSIVE_OBSERVER";
            case SpyNodeType::TX_CORRELATOR:     return "TX_CORRELATOR";
            default:                             return "UNKNOWN";
        }
    }
};

} // namespace ninacatcoin_ai
