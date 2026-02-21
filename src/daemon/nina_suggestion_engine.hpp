// Copyright (c) 2026, The ninacatcoin Project
// NINA Suggestion Engine Header

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <ctime>
#include <sstream>

namespace ninacatcoin_ai {

enum class SuggestionType {
    ATTACK_PATTERN_MITIGATION,
    PEER_ISOLATION,
    CONSENSUS_PROTECTION,
    PERFORMANCE_OPTIMIZATION,
    SECURITY_HARDENING,
    DEFENSE_IMPROVEMENT
};

struct Suggestion {
    std::string suggestion_id;
    SuggestionType type;
    std::string title;
    std::string description;
    std::vector<std::string> rationale;
    double confidence = 0.0;
    std::string requested_change;
    time_t creation_timestamp = 0;
    std::string constitutional_check = "";
    bool is_approved_by_human = false;
    time_t approval_timestamp = 0;
    std::string approved_by = "";
    
    // Serialización para persistencia en LMDB
    std::string serialize() const {
        std::stringstream ss;
        ss << suggestion_id << "|" << static_cast<int>(type) << "|" << title << "|"
           << description << "|" << confidence << "|" << requested_change << "|"
           << creation_timestamp << "|" << is_approved_by_human << "|"
           << approval_timestamp << "|" << approved_by;
        return ss.str();
    }
};

class NINASuggestionEngine {
public:
    static NINASuggestionEngine& getInstance();
    
    bool initialize();
    
    Suggestion* createSuggestion(
        SuggestionType type,
        const std::string& title,
        const std::string& description,
        const std::vector<std::string>& rationale,
        double confidence,
        const std::string& requested_change
    );
    
    bool validateAgainstConstitution(const Suggestion& suggestion);
    bool approveSuggestion(const std::string& suggestion_id, const std::string& approved_by);
    bool rejectSuggestion(const std::string& suggestion_id, const std::string& rejection_reason);
    bool executeApprovedSuggestion(const std::string& suggestion_id);
    
    const std::vector<Suggestion>& getPendingSuggestions() const;
    const std::vector<Suggestion>& getHistoricalSuggestions() const;
    
    std::string getCurrentThinking() const;
    double getApprovalRate() const;
    bool isConstitutional(const std::string& proposed_change);
    
    // ============ MÉTODOS DE PERSISTENCIA ============
    // Guardar estado en LMDB (~/.ninacatcoin/lmdb/data.mdb)
    bool persistToLMDB(uint64_t current_height = 0);
    // Cargar estado desde LMDB en startup
    bool loadFromLMDB();
    // Obtener estadísticas de persistencia
    uint64_t getPendingCount() const { return pending_suggestions.size(); }
    uint64_t getHistoricalCount() const { return historical_suggestions.size(); }
    
private:
    NINASuggestionEngine();
    ~NINASuggestionEngine();
    std::string generateSuggestionID();
    
    std::vector<Suggestion> pending_suggestions;
    std::vector<Suggestion> historical_suggestions;
    uint64_t total_suggestions = 0;
    uint64_t approved_suggestions = 0;
    uint64_t rejected_suggestions = 0;
    
    // Tracking de persistencia
    uint64_t last_persist_height = 0;
};

} // namespace ninacatcoin_ai
