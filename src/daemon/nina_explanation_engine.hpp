/**
 * NINA Explanation Engine - TIER 4
 * Every decision is justified and traceable
 * Builds trust through transparency
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * Decision Explanation Structure
 * Documents WHY NINA made a specific decision
 */
struct DecisionExplanation {
    std::string decision_id;           // Unique identifier
    std::string action_type;            // What NINA decided
    std::string reasoning;              // Plain English explanation
    std::vector<std::string> evidence;  // Facts supporting decision
    std::vector<std::string> constraints; // Rules that applied
    double confidence_score;            // 0.0 to 1.0
    std::time_t timestamp;
    std::string escalation_level;       // "AUTO", "WARN", "HUMAN_REQUIRED"
};

class NInaExplanationEngine {
public:
    /**
     * Explain a blockchain validation decision
     */
    static DecisionExplanation explain_block_validation(
        int block_height,
        bool is_valid,
        std::vector<std::string> validation_rules,
        double anomaly_score
    );

    /**
     * Explain a checkpoint validation decision
     */
    static DecisionExplanation explain_checkpoint_validation(
        int checkpoint_height,
        bool sources_verified,
        int seeds_confirmed,
        bool potential_attack
    );

    /**
     * Explain a transaction filtering decision
     */
    static DecisionExplanation explain_transaction_filtering(
        const std::string& tx_hash,
        bool should_relay,
        std::vector<std::string> anomaly_flags,
        double suspicion_score
    );

    /**
     * Explain a network peer evaluation
     */
    static DecisionExplanation explain_peer_reputation(
        const std::string& peer_id,
        double reputation_score,
        std::vector<std::string> behaviors_observed,
        bool is_trustworthy
    );

    /**
     * Convert explanation to human-readable format
     */
    static std::string format_explanation(const DecisionExplanation& exp);

    /**
     * Log explanation for audit trail
     */
    static void log_decision(const DecisionExplanation& exp);

    /**
     * Get explanation history for accountability
     */
    static std::vector<DecisionExplanation> get_decision_history(
        const std::string& action_type,
        int limit = 100
    );
};

} // namespace ninacatcoin_ai
