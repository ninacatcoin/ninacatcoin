#pragma once

#include "nina_persistence_engine.hpp"
#include <string>
#include <memory>

namespace ninacatcoin_ai {

/**
 * NINA Persistence API - Simplified interface for TIER access to persistence
 * Provides high-level methods for saving TIER-specific data
 */
class NINAPersistenceAPI {
public:
    // ========== TIER 4: EXPLANATION ENGINE API ==========
    
    /**
     * Log a block validation decision with full reasoning
     */
    static bool log_block_validation_decision(
        int block_height,
        bool is_valid,
        const std::string& reasoning,
        double confidence_score,
        const std::vector<std::string>& evidence
    ) {
        DecisionRecord record;
        record.decision_id = "BLOCK_" + std::to_string(block_height) + "_" + std::to_string(std::time(nullptr));
        record.timestamp = std::time(nullptr);
        record.block_height = block_height;
        record.decision_type = "BLOCK_VALIDATION";
        record.action_taken = is_valid ? "ACCEPT_BLOCK" : "REJECT_BLOCK";
        record.reasoning = reasoning;
        record.evidence = evidence;
        record.confidence_score = confidence_score;
        record.was_successful = true;
        record.outcome_summary = is_valid ? "Block accepted by NINA validation" : "Block rejected - failed NINA checks";
        
        return NINaPersistenceEngine::save_decision_record(record);
    }
    
    /**
     * Log a checkpoint validation decision
     */
    static bool log_checkpoint_validation_decision(
        int checkpoint_height,
        const std::string& checkpoint_hash,
        bool is_valid,
        const std::string& reasoning,
        double confidence_score
    ) {
        DecisionRecord record;
        record.decision_id = "CHECKPOINT_" + std::to_string(checkpoint_height) + "_" + std::to_string(std::time(nullptr));
        record.timestamp = std::time(nullptr);
        record.block_height = checkpoint_height;
        record.decision_type = "CHECKPOINT_VALIDATION";
        record.action_taken = is_valid ? "ACCEPT_CHECKPOINT" : "REJECT_CHECKPOINT";
        record.reasoning = reasoning + " | Hash: " + checkpoint_hash;
        record.evidence = {"Hash verified", "Timestamp valid", "Epoch aligned"};
        record.confidence_score = confidence_score;
        record.was_successful = is_valid;
        record.outcome_summary = is_valid ? "Checkpoint validated" : "Checkpoint rejected";
        
        return NINaPersistenceEngine::save_decision_record(record);
    }
    
    /**
     * Log a transaction filtering decision
     */
    static bool log_transaction_filtering_decision(
        const std::string& tx_id,
        bool is_valid,
        const std::string& filter_reason,
        double confidence_score
    ) {
        DecisionRecord record;
        record.decision_id = "TX_" + tx_id.substr(0, 16) + "_" + std::to_string(std::time(nullptr));
        record.timestamp = std::time(nullptr);
        record.block_height = 0;
        record.decision_type = "TRANSACTION_FILTERING";
        record.action_taken = is_valid ? "ALLOW_TX" : "FILTER_TX";
        record.reasoning = filter_reason;
        record.evidence = {"Mempool checked", "Fee verified", "Signature valid"};
        record.confidence_score = confidence_score;
        record.was_successful = !is_valid;
        record.outcome_summary = is_valid ? "Transaction forwarded to network" : "Transaction filtered: " + filter_reason;
        
        return NINaPersistenceEngine::save_decision_record(record);
    }
    
    // ========== TIER 3: GOVERNANCE ENGINE API ==========
    
    /**
     * Log a governance proposal suggestion
     */
    static bool log_governance_proposal(
        const std::string& proposal_type,
        const std::string& current_value,
        const std::string& proposed_value,
        const std::vector<std::string>& affected_systems,
        const std::string& justification,
        bool is_constitutional
    ) {
        GovernanceProposalRecord record;
        record.proposal_id = "PROP_" + proposal_type + "_" + std::to_string(std::time(nullptr));
        record.created_timestamp = std::time(nullptr);
        record.proposal_type = proposal_type;
        record.current_value = current_value;
        record.proposed_value = proposed_value;
        record.affected_systems = affected_systems;
        record.justification = justification;
        record.is_constitutional = is_constitutional;
        record.votes_for = 0;
        record.votes_against = 0;
        record.status = "PENDING";
        
        return NINaPersistenceEngine::save_governance_proposal(record);
    }
    
    /**
     * Update proposal status (PENDING -> APPROVED / REJECTED / IMPLEMENTED)
     */
    static bool update_governance_status(const std::string& proposal_id, const std::string& new_status) {
        return NINaPersistenceEngine::update_proposal_status(proposal_id, new_status);
    }
    
    // ========== TIER 5: ADAPTIVE LEARNING API ==========
    
    /**
     * Log a discovered attack pattern
     */
    static bool log_attack_pattern(
        const std::string& pattern_type,
        const std::string& description,
        int occurrence_count,
        double confidence,
        const std::vector<std::string>& affected_blocks
    ) {
        LearningPatternRecord record;
        record.pattern_id = "PATTERN_ATTACK_" + std::to_string(std::time(nullptr));
        record.discovered_timestamp = std::time(nullptr);
        record.pattern_type = pattern_type;
        record.pattern_description = description;
        record.occurrence_count = occurrence_count;
        record.confidence = confidence;
        record.affected_metric = "security_threat_level";
        record.affected_blocks = affected_blocks;
        record.improvement_recommendation = "Increase validator scrutiny for this pattern type";
        
        return NINaPersistenceEngine::save_learning_pattern(record);
    }
    
    /**
     * Log a behavioral baseline discovery
     */
    static bool log_baseline_pattern(
        const std::string& metric_name,
        const std::string& description,
        int occurrence_count,
        double confidence
    ) {
        LearningPatternRecord record;
        record.pattern_id = "PATTERN_BASELINE_" + metric_name + "_" + std::to_string(std::time(nullptr));
        record.discovered_timestamp = std::time(nullptr);
        record.pattern_type = "BASELINE_BEHAVIOR";
        record.pattern_description = description;
        record.occurrence_count = occurrence_count;
        record.confidence = confidence;
        record.affected_metric = metric_name;
        record.improvement_recommendation = "Use as reference for anomaly detection";
        
        return NINaPersistenceEngine::save_learning_pattern(record);
    }
    
    /**
     * Log a difficulty prediction
     */
    static bool log_difficulty_prediction(
        int current_height,
        int predicted_next_difficulty,
        double accuracy_score,
        const std::string& reasoning
    ) {
        LearningPatternRecord record;
        record.pattern_id = "PREDICTION_DIFFICULTY_" + std::to_string(current_height) + "_" + std::to_string(std::time(nullptr));
        record.discovered_timestamp = std::time(nullptr);
        record.pattern_type = "DIFFICULTY_PREDICTION";
        record.pattern_description = "Predicted next difficulty: " + std::to_string(predicted_next_difficulty);
        record.occurrence_count = 1;
        record.confidence = accuracy_score;
        record.affected_metric = "difficulty_adjustment";
        record.improvement_recommendation = reasoning;
        
        return NINaPersistenceEngine::save_learning_pattern(record);
    }
    
    // ========== TIER 6: HUMAN COLLABORATION API ==========
    
    /**
     * Log an escalation for human review
     */
    static bool create_escalation(
        const std::string& situation_description,
        const std::vector<std::string>& options,
        const std::string& nina_recommendation,
        const std::string& urgency_level
    ) {
        EscalationRecord record;
        record.escalation_id = "ESC_" + std::to_string(std::time(nullptr));
        record.created_timestamp = std::time(nullptr);
        record.situation_description = situation_description;
        record.options = options;
        record.nina_recommendation = nina_recommendation;
        record.urgency_level = urgency_level;
        record.resolved = false;
        record.human_decision = "";
        record.resolved_timestamp = 0;
        
        return NINaPersistenceEngine::save_escalation(record);
    }
    
    /**
     * Retrieve pending escalations for human action
     */
    static bool get_pending_escalations(std::vector<EscalationRecord>& out_escalations) {
        return NINaPersistenceEngine::get_pending_escalations(out_escalations);
    }
    
    /**
     * Resolve an escalation with human decision
     */
    static bool resolve_escalation(const std::string& escalation_id, const std::string& human_decision) {
        return NINaPersistenceEngine::resolve_escalation(escalation_id, human_decision);
    }
    
    // ========== ANALYTICS & STATISTICS ==========
    
    /**
     * Get system-wide statistics
     */
    static bool get_system_statistics(
        int& total_decisions,
        double& avg_confidence,
        int& successful_decisions,
        int& total_proposals,
        int& total_patterns_learned
    ) {
        if (!NINaPersistenceEngine::get_decision_statistics(total_decisions, avg_confidence, successful_decisions)) {
            return false;
        }
        
        // Additional stats retrieval would go here
        total_proposals = 0;
        total_patterns_learned = 0;
        
        return true;
    }
};

}  // namespace ninacatcoin_ai
