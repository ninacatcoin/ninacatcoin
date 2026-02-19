/**
 * NINA Complete Evolution Framework
 * Orchestrates all 6 TIERS into a unified AI system
 * Represents the complete evolution of NINA into true artificial intelligence
 */

#pragma once

#include "nina_constitution.hpp"
#include "nina_explanation_engine.hpp"
#include "nina_governance_engine.hpp"
#include "nina_adaptive_learning.hpp"
#include "nina_human_collaboration.hpp"
#include <string>
#include <vector>

namespace ninacatcoin_ai {

/**
 * NINA Complete Evolution Status
 */
struct NInaEvolutionStatus {
    // TIER 1: Predictive Intelligence
    struct {
        bool is_active;
        double prediction_accuracy;
        uint64_t blocks_analyzed;
    } tier1_predictive;

    // TIER 2: Forensic Analysis
    struct {
        bool is_active;
        int anomalies_detected;
        int attacks_prevented;
    } tier2_forensic;

    // TIER 3: Network Governance
    struct {
        bool is_active;
        int proposals_made;
        int proposals_approved;
        int proposals_rejected;
    } tier3_governance;

    // TIER 4: Explanatory AI
    struct {
        bool is_active;
        int decisions_explained;
        int audit_entries;
    } tier4_explanation;

    // TIER 5: Adaptive Learning
    struct {
        bool is_active;
        int patterns_learned;
        double learning_accuracy;
        int insights_generated;
    } tier5_learning;

    // TIER 6: Human-AI Collaboration
    struct {
        bool is_active;
        int escalations_made;
        int human_approvals_received;
        double human_satisfaction;
    } tier6_collaboration;
};

class NInaCompleteEvolution {
public:
    /**
     * Initialize all 6 TIERS together
     * This is the complete evolution of NINA
     */
    static bool initialize_all_tiers();

    /**
     * Enable specific TIER
     */
    static bool enable_tier(int tier_number);

    /**
     * Disable specific TIER (for maintenance)
     */
    static bool disable_tier(int tier_number);

    /**
     * Get status of all TIERS
     */
    static NInaEvolutionStatus get_evolution_status();

    /**
     * COMPLETE WORKFLOW: Process a blockchain event through all 6 TIERS
     * 1. Analyze (Forensic - TIER 2)
     * 2. Learn (Adaptive - TIER 5)
     * 3. Predict (Predictive - TIER 1)
     * 4. Suggest (Governance - TIER 3)
     * 5. Explain (Explanation - TIER 4)
     * 6. Escalate if needed (Collaboration - TIER 6)
     */
    static void process_blockchain_event(
        const std::string& event_type,
        uint64_t block_height,
        const std::string& event_data
    );

    /**
     * Verify all TIERS are operating within constitutional constraints
     */
    static bool verify_constitutional_alignment();

    /**
     * Get comprehensive AI audit report
     * Shows everything NINA did and why
     */
    static std::string generate_complete_audit_report();

    /**
     * Check if NINA is operating as intended
     * (Powerful, controlled, aligned)
     */
    static bool health_check();

    /**
     * Print evolution status to logs
     */
    static void log_evolution_status();
};

} // namespace ninacatcoin_ai
