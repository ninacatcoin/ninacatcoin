#ifndef NINA_PERSISTENCE_ENGINE_HPP
#define NINA_PERSISTENCE_ENGINE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace ninacatcoin_ai {

// Data structures for persistence
struct DecisionRecord {
    std::string decision_id;
    int64_t timestamp;
    int block_height;
    std::string decision_type;      // e.g., "BLOCK_VALIDATION", "TX_FILTER"
    std::string action_taken;        // e.g., "ACCEPT", "REJECT", "ESCALATE"
    std::string reasoning;
    double confidence_score;
    bool was_successful;
    std::string outcome_summary;
    std::vector<std::string> evidence;
};

struct GovernanceProposalRecord {
    std::string proposal_id;
    int64_t created_timestamp;
    std::string proposal_type;       // e.g., "PARAMETER_CHANGE", "FEATURE_ENABLE"
    std::string current_value;
    std::string proposed_value;
    std::string justification;
    bool is_constitutional;
    int votes_for;
    int votes_against;
    std::string status;              // "PENDING", "APPROVED", "REJECTED"
    std::vector<std::string> affected_systems;
};

struct LearningPatternRecord {
    std::string pattern_id;
    int64_t discovered_timestamp;
    std::string pattern_type;        // e.g., "ATTACK_PATTERN", "DIFFICULTY_TREND"
    std::string pattern_description;
    int occurrence_count;
    double confidence;
    std::string affected_metric;
    std::string improvement_recommendation;
    std::vector<std::string> affected_blocks;
};

struct EscalationRecord {
    std::string escalation_id;
    int64_t created_timestamp;
    std::string situation_description;
    std::string nina_recommendation;
    std::string urgency_level;       // "LOW", "MEDIUM", "HIGH", "CRITICAL"
    bool resolved;
    std::string human_decision;      // Human's choice from the options
    int64_t resolved_timestamp;      // 0 if not resolved
    std::vector<std::string> options; // Decision options presented to human
};

class NINaPersistenceEngine {
public:
    // Initialization
    static bool initialize(const std::string& path);
    static bool shutdown();
    
    // Decision Records (TIER 4 Security)
    static bool save_decision_record(const DecisionRecord& record);
    static bool get_decision_record(const std::string& decision_id, DecisionRecord& out_record);
    static bool get_all_decisions(std::vector<DecisionRecord>& out_records);
    static bool get_decisions_by_height(int block_height, std::vector<DecisionRecord>& out_records);
    
    // Governance Proposals (TIER 3)
    static bool save_governance_proposal(const GovernanceProposalRecord& record);
    static bool get_governance_proposal(const std::string& proposal_id, GovernanceProposalRecord& out_record);
    static bool get_all_proposals(std::vector<GovernanceProposalRecord>& out_records);
    static bool update_proposal_status(const std::string& proposal_id, const std::string& new_status);
    
    // Learning Patterns (TIER 5 Analytics)
    static bool save_learning_pattern(const LearningPatternRecord& record);
    static bool get_learning_pattern(const std::string& pattern_id, LearningPatternRecord& out_record);
    static bool get_all_patterns(std::vector<LearningPatternRecord>& out_records);
    static bool get_patterns_by_type(const std::string& pattern_type, std::vector<LearningPatternRecord>& out_records);
    
    // Escalations (TIER 6 Human Oversight)
    static bool save_escalation(const EscalationRecord& record);
    static bool get_escalation(const std::string& escalation_id, EscalationRecord& out_record);
    static bool get_all_escalations(std::vector<EscalationRecord>& out_records);
    static bool get_pending_escalations(std::vector<EscalationRecord>& out_records);
    static bool resolve_escalation(const std::string& escalation_id, const std::string& human_decision);
    
    // Statistics & Analytics
    static bool get_decision_statistics(int& total_decisions, double& avg_confidence, int& successful_count);
    static bool get_learning_progress(int& total_patterns, int& patterns_this_session);
    static bool get_system_audit_trail(std::vector<std::pair<int64_t, std::string>>& out_trail);
    
    // Maintenance
    static bool clear_old_records(int64_t before_timestamp);

private:
    static std::string db_path;
    static bool is_initialized;
    
    // Serialization helpers
    static std::string serialize_decision_record(const DecisionRecord& record);
    static DecisionRecord deserialize_decision_record(const std::string& data);
    
    static std::string serialize_proposal_record(const GovernanceProposalRecord& record);
    static GovernanceProposalRecord deserialize_proposal_record(const std::string& data);
    
    static std::string serialize_pattern_record(const LearningPatternRecord& record);
    static LearningPatternRecord deserialize_pattern_record(const std::string& data);
    
    static std::string serialize_escalation_record(const EscalationRecord& record);
    static EscalationRecord deserialize_escalation_record(const std::string& data);
};

}  // namespace ninacatcoin_ai

#endif  // NINA_PERSISTENCE_ENGINE_HPP
