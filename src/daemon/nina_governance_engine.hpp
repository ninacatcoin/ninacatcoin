/**
 * NINA Network Governance Engine - TIER 3
 * Self-governance WITHOUT escaping constraints
 * Makes suggestions, humans approve
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace ninacatcoin_ai {

/**
 * Governance Proposal Structure
 * NINA suggests, humans decide
 */
struct GovernanceProposal {
    std::string proposal_id;
    std::string proposal_type;          // "PARAM_ADJUSTMENT", "RULE_IMPROVEMENT", etc.
    std::string description;
    std::string current_value;
    std::string proposed_value;
    std::string justification;
    double expected_improvement;       // 0.0 to 1.0
    std::vector<std::string> affected_systems;
    bool requires_consensus;
    std::string status;                // "PROPOSED", "VOTING", "APPROVED", "REJECTED"
};

class NInaNetworkGovernance {
public:
    /**
     * Suggest difficulty adjustment based on network conditions
     * DOES NOT EXECUTE - only suggests
     */
    static GovernanceProposal suggest_difficulty_adjustment(
        uint64_t block_height,
        double current_difficulty,
        double target_blocktime,
        double actual_blocktime
    );

    /**
     * Suggest fee optimization
     * Based on mempool analysis and network congestion
     */
    static GovernanceProposal suggest_fee_optimization(
        uint64_t pending_transactions,
        double current_avg_fee,
        double network_capacity
    );

    /**
     * Suggest peer connection optimization
     */
    static GovernanceProposal suggest_peer_optimization(
        int current_peer_count,
        int optimal_peer_count,
        double network_health_score
    );

    /**
     * Suggest checkpoint interval adjustment
     */
    static GovernanceProposal suggest_checkpoint_interval(
        uint64_t blocks_since_last_checkpoint,
        double network_stability_score
    );

    /**
     * Evaluate if a proposal violates constitutional constraints
     */
    static bool is_proposal_constitutional(const GovernanceProposal& proposal);

    /**
     * Submit proposal to human validators
     * Returns true if humans approve
     */
    static bool submit_proposal_for_approval(const GovernanceProposal& proposal);

    /**
     * Get all active proposals
     */
    static std::vector<GovernanceProposal> get_active_proposals();

    /**
     * Track performance of implemented proposals
     */
    static void track_proposal_impact(
        const std::string& proposal_id,
        double actual_improvement
    );
};

} // namespace ninacatcoin_ai
