#include "nina_persistence_engine.hpp"
#include "misc_log_ex.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "NINA-PERSISTENCE"

namespace ninacatcoin_ai {

// Static member initialization
std::string NINaPersistenceEngine::db_path = "";
bool NINaPersistenceEngine::is_initialized = false;

bool NINaPersistenceEngine::initialize(const std::string& path) {
    try {
        db_path = path;
        
        // Create directory if it doesn't exist
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) {
            MERROR("Failed to create persistence directory: " << path << " - " << ec.message());
            return false;
        }
        
        is_initialized = true;
        MINFO("[NINA-PERSISTENCE] ✓ Manager initialized");
        MINFO("[NINA-PERSISTENCE] ✓ Storage path: " << path);
        MINFO("[NINA-PERSISTENCE] ✓ Loading previous state from disk...");
        
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception during persistence initialization: " << e.what());
        return false;
    }
}

std::string NINaPersistenceEngine::serialize_decision_record(const DecisionRecord& record) {
    std::stringstream ss;
    ss << record.decision_id << "|"
       << record.timestamp << "|"
       << record.block_height << "|"
       << record.decision_type << "|"
       << record.action_taken << "|"
       << record.reasoning << "|"
       << record.confidence_score << "|"
       << (record.was_successful ? "1" : "0") << "|"
       << record.outcome_summary << "|";
       
    for (size_t i = 0; i < record.evidence.size(); ++i) {
        ss << record.evidence[i];
        if (i < record.evidence.size() - 1) ss << ",";
    }
    
    return ss.str();
}

DecisionRecord NINaPersistenceEngine::deserialize_decision_record(const std::string& data) {
    DecisionRecord record;
    std::stringstream ss(data);
    std::string line, token;
    
    std::getline(ss, record.decision_id, '|');
    std::getline(ss, token, '|'); record.timestamp = std::stoll(token);
    std::getline(ss, token, '|'); record.block_height = std::stoi(token);
    std::getline(ss, record.decision_type, '|');
    std::getline(ss, record.action_taken, '|');
    std::getline(ss, record.reasoning, '|');
    std::getline(ss, token, '|'); record.confidence_score = std::stod(token);
    std::getline(ss, token, '|'); record.was_successful = (token == "1");
    std::getline(ss, record.outcome_summary, '|');
    
    std::getline(ss, line);
    std::stringstream ev_ss(line);
    std::string ev;
    while (std::getline(ev_ss, ev, ',')) {
        record.evidence.push_back(ev);
    }
    
    return record;
}

bool NINaPersistenceEngine::save_decision_record(const DecisionRecord& record) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/decisions.json";
        std::ofstream file(filepath, std::ios::app);
        if (!file.is_open()) return false;
        
        file << serialize_decision_record(record) << "\n";
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception saving decision record: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_decision_record(const std::string& decision_id, DecisionRecord& out_record) {
    if (!is_initialized) return false;
    
    try {
        std::vector<DecisionRecord> records;
        if (get_all_decisions(records)) {
            for (const auto& rec : records) {
                if (rec.decision_id == decision_id) {
                    out_record = rec;
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting decision record: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_all_decisions(std::vector<DecisionRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/decisions.json";
        std::ifstream file(filepath);
        if (!file.is_open()) return true;  // File doesn't exist yet
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                out_records.push_back(deserialize_decision_record(line));
            }
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting all decisions: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_decisions_by_height(int block_height, std::vector<DecisionRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::vector<DecisionRecord> all_records;
        if (get_all_decisions(all_records)) {
            for (const auto& rec : all_records) {
                if (rec.block_height == block_height) {
                    out_records.push_back(rec);
                }
            }
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting decisions by height: " << e.what());
        return false;
    }
}

std::string NINaPersistenceEngine::serialize_proposal_record(const GovernanceProposalRecord& record) {
    std::stringstream ss;
    ss << record.proposal_id << "|"
       << record.created_timestamp << "|"
       << record.proposal_type << "|"
       << record.current_value << "|"
       << record.proposed_value << "|"
       << record.justification << "|"
       << (record.is_constitutional ? "1" : "0") << "|"
       << record.votes_for << "|"
       << record.votes_against << "|"
       << record.status << "|";
       
    for (size_t i = 0; i < record.affected_systems.size(); ++i) {
        ss << record.affected_systems[i];
        if (i < record.affected_systems.size() - 1) ss << ",";
    }
    
    return ss.str();
}

GovernanceProposalRecord NINaPersistenceEngine::deserialize_proposal_record(const std::string& data) {
    GovernanceProposalRecord record;
    std::stringstream ss(data);
    std::string token;
    
    std::getline(ss, record.proposal_id, '|');
    std::getline(ss, token, '|'); record.created_timestamp = std::stoll(token);
    std::getline(ss, record.proposal_type, '|');
    std::getline(ss, record.current_value, '|');
    std::getline(ss, record.proposed_value, '|');
    std::getline(ss, record.justification, '|');
    std::getline(ss, token, '|'); record.is_constitutional = (token == "1");
    std::getline(ss, token, '|'); record.votes_for = std::stoi(token);
    std::getline(ss, token, '|'); record.votes_against = std::stoi(token);
    std::getline(ss, record.status, '|');
    
    std::string line;
    std::getline(ss, line);
    std::stringstream sys_ss(line);
    std::string sys;
    while (std::getline(sys_ss, sys, ',')) {
        record.affected_systems.push_back(sys);
    }
    
    return record;
}

bool NINaPersistenceEngine::save_governance_proposal(const GovernanceProposalRecord& record) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/proposals.json";
        std::ofstream file(filepath, std::ios::app);
        if (!file.is_open()) return false;
        
        file << serialize_proposal_record(record) << "\n";
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception saving governance proposal: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_governance_proposal(const std::string& proposal_id, GovernanceProposalRecord& out_record) {
    if (!is_initialized) return false;
    
    try {
        std::vector<GovernanceProposalRecord> records;
        if (get_all_proposals(records)) {
            for (const auto& rec : records) {
                if (rec.proposal_id == proposal_id) {
                    out_record = rec;
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting governance proposal: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_all_proposals(std::vector<GovernanceProposalRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/proposals.json";
        std::ifstream file(filepath);
        if (!file.is_open()) return true;
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                out_records.push_back(deserialize_proposal_record(line));
            }
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting all proposals: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::update_proposal_status(const std::string& proposal_id, const std::string& new_status) {
    GovernanceProposalRecord record;
    if (!get_governance_proposal(proposal_id, record)) return false;
    
    record.status = new_status;
    return save_governance_proposal(record);
}

std::string NINaPersistenceEngine::serialize_pattern_record(const LearningPatternRecord& record) {
    std::stringstream ss;
    ss << record.pattern_id << "|"
       << record.discovered_timestamp << "|"
       << record.pattern_type << "|"
       << record.pattern_description << "|"
       << record.occurrence_count << "|"
       << record.confidence << "|"
       << record.affected_metric << "|"
       << record.improvement_recommendation << "|";
       
    for (size_t i = 0; i < record.affected_blocks.size(); ++i) {
        ss << record.affected_blocks[i];
        if (i < record.affected_blocks.size() - 1) ss << ",";
    }
    
    return ss.str();
}

LearningPatternRecord NINaPersistenceEngine::deserialize_pattern_record(const std::string& data) {
    LearningPatternRecord record;
    std::stringstream ss(data);
    std::string token;
    
    std::getline(ss, record.pattern_id, '|');
    std::getline(ss, token, '|'); record.discovered_timestamp = std::stoll(token);
    std::getline(ss, record.pattern_type, '|');
    std::getline(ss, record.pattern_description, '|');
    std::getline(ss, token, '|'); record.occurrence_count = std::stoi(token);
    std::getline(ss, token, '|'); record.confidence = std::stod(token);
    std::getline(ss, record.affected_metric, '|');
    std::getline(ss, record.improvement_recommendation, '|');
    
    std::string line;
    std::getline(ss, line);
    std::stringstream blk_ss(line);
    std::string blk;
    while (std::getline(blk_ss, blk, ',')) {
        record.affected_blocks.push_back(blk);
    }
    
    return record;
}

bool NINaPersistenceEngine::save_learning_pattern(const LearningPatternRecord& record) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/patterns.json";
        std::ofstream file(filepath, std::ios::app);
        if (!file.is_open()) return false;
        
        file << serialize_pattern_record(record) << "\n";
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception saving learning pattern: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_learning_pattern(const std::string& pattern_id, LearningPatternRecord& out_record) {
    if (!is_initialized) return false;
    
    try {
        std::vector<LearningPatternRecord> records;
        if (get_all_patterns(records)) {
            for (const auto& rec : records) {
                if (rec.pattern_id == pattern_id) {
                    out_record = rec;
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting learning pattern: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_all_patterns(std::vector<LearningPatternRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/patterns.json";
        std::ifstream file(filepath);
        if (!file.is_open()) return true;
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                out_records.push_back(deserialize_pattern_record(line));
            }
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting all patterns: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_patterns_by_type(const std::string& pattern_type, std::vector<LearningPatternRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::vector<LearningPatternRecord> all_records;
        if (get_all_patterns(all_records)) {
            for (const auto& rec : all_records) {
                if (rec.pattern_type == pattern_type) {
                    out_records.push_back(rec);
                }
            }
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting patterns by type: " << e.what());
        return false;
    }
}

std::string NINaPersistenceEngine::serialize_escalation_record(const EscalationRecord& record) {
    std::stringstream ss;
    ss << record.escalation_id << "|"
       << record.created_timestamp << "|"
       << record.situation_description << "|"
       << record.nina_recommendation << "|"
       << record.urgency_level << "|"
       << (record.resolved ? "1" : "0") << "|"
       << record.human_decision << "|"
       << record.resolved_timestamp << "|";
       
    for (size_t i = 0; i < record.options.size(); ++i) {
        ss << record.options[i];
        if (i < record.options.size() - 1) ss << ",";
    }
    
    return ss.str();
}

EscalationRecord NINaPersistenceEngine::deserialize_escalation_record(const std::string& data) {
    EscalationRecord record;
    std::stringstream ss(data);
    std::string token;
    
    std::getline(ss, record.escalation_id, '|');
    std::getline(ss, token, '|'); record.created_timestamp = std::stoll(token);
    std::getline(ss, record.situation_description, '|');
    std::getline(ss, record.nina_recommendation, '|');
    std::getline(ss, record.urgency_level, '|');
    std::getline(ss, token, '|'); record.resolved = (token == "1");
    std::getline(ss, record.human_decision, '|');
    std::getline(ss, token, '|'); record.resolved_timestamp = std::stoll(token);
    
    std::string line;
    std::getline(ss, line);
    std::stringstream opt_ss(line);
    std::string opt;
    while (std::getline(opt_ss, opt, ',')) {
        record.options.push_back(opt);
    }
    
    return record;
}

bool NINaPersistenceEngine::save_escalation(const EscalationRecord& record) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/escalations.json";
        std::ofstream file(filepath, std::ios::app);
        if (!file.is_open()) return false;
        
        file << serialize_escalation_record(record) << "\n";
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception saving escalation: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_escalation(const std::string& escalation_id, EscalationRecord& out_record) {
    if (!is_initialized) return false;
    
    try {
        std::vector<EscalationRecord> records;
        if (get_all_escalations(records)) {
            for (const auto& rec : records) {
                if (rec.escalation_id == escalation_id) {
                    out_record = rec;
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting escalation: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_all_escalations(std::vector<EscalationRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::string filepath = db_path + "/escalations.json";
        std::ifstream file(filepath);
        if (!file.is_open()) return true;
        
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                out_records.push_back(deserialize_escalation_record(line));
            }
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting all escalations: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_pending_escalations(std::vector<EscalationRecord>& out_records) {
    if (!is_initialized) return false;
    
    try {
        std::vector<EscalationRecord> all_records;
        if (get_all_escalations(all_records)) {
            for (const auto& rec : all_records) {
                if (!rec.resolved) {
                    out_records.push_back(rec);
                }
            }
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting pending escalations: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::resolve_escalation(const std::string& escalation_id, const std::string& human_decision) {
    EscalationRecord record;
    if (!get_escalation(escalation_id, record)) return false;
    
    record.resolved = true;
    record.human_decision = human_decision;
    record.resolved_timestamp = std::time(nullptr);
    
    return save_escalation(record);
}

bool NINaPersistenceEngine::get_decision_statistics(int& total_decisions, double& avg_confidence, int& successful_count) {
    if (!is_initialized) return false;
    
    try {
        std::vector<DecisionRecord> records;
        if (!get_all_decisions(records)) return false;
        
        total_decisions = records.size();
        successful_count = 0;
        double total_confidence = 0.0;
        
        for (const auto& rec : records) {
            if (rec.was_successful) successful_count++;
            total_confidence += rec.confidence_score;
        }
        
        avg_confidence = (total_decisions > 0) ? (total_confidence / total_decisions) : 0.0;
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting decision statistics: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_learning_progress(int& total_patterns, int& patterns_this_session) {
    if (!is_initialized) return false;
    
    try {
        std::vector<LearningPatternRecord> records;
        if (get_all_patterns(records)) {
            total_patterns = records.size();
            patterns_this_session = 0;  // Would be tracked separately
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        MERROR("Exception getting learning progress: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::get_system_audit_trail(std::vector<std::pair<int64_t, std::string>>& out_trail) {
    if (!is_initialized) return false;
    
    try {
        std::vector<DecisionRecord> decisions;
        std::vector<GovernanceProposalRecord> proposals;
        std::vector<LearningPatternRecord> patterns;
        std::vector<EscalationRecord> escalations;
        
        get_all_decisions(decisions);
        get_all_proposals(proposals);
        get_all_patterns(patterns);
        get_all_escalations(escalations);
        
        for (const auto& dec : decisions) {
            out_trail.push_back({dec.timestamp, "DECISION: " + dec.decision_type});
        }
        for (const auto& prop : proposals) {
            out_trail.push_back({prop.created_timestamp, "PROPOSAL: " + prop.proposal_type});
        }
        for (const auto& pat : patterns) {
            out_trail.push_back({pat.discovered_timestamp, "PATTERN: " + pat.pattern_type});
        }
        for (const auto& esc : escalations) {
            out_trail.push_back({esc.created_timestamp, "ESCALATION: " + esc.urgency_level});
        }
        
        // Sort by timestamp
        std::sort(out_trail.begin(), out_trail.end());
        
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception getting system audit trail: " << e.what());
        return false;
    }
}

bool NINaPersistenceEngine::clear_old_records(int64_t before_timestamp) {
    // Simplified - would implement proper cleanup
    return true;
}

bool NINaPersistenceEngine::shutdown() {
    if (!is_initialized) return true;
    
    try {
        is_initialized = false;
        MINFO("[NINA-PERSISTENCE] ✓ Persistence engine shut down");
        return true;
    } catch (const std::exception& e) {
        MERROR("Exception during persistence engine shutdown: " << e.what());
        return false;
    }
}

}  // namespace ninacatcoin_ai
