// Copyright (c) 2026, The ninacatcoin Project
// NINA Memory System Header

#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cstdint>
#include <sstream>

namespace ninacatcoin_ai {

struct AttackPattern {
    std::string pattern_name;
    uint64_t occurrence_count = 0;
    double confidence = 0.0;
    double severity_average = 0.0;
    time_t first_seen = 0;
    time_t last_seen = 0;
    
    // Serialización para persistencia en LMDB
    std::string serialize() const {
        std::stringstream ss;
        ss << pattern_name << "|" << occurrence_count << "|" << confidence << "|"
           << severity_average << "|" << first_seen << "|" << last_seen;
        return ss.str();
    }
};

struct PeerBehavior {
    std::string peer_ip;
    uint64_t total_interactions = 0;
    uint64_t suspicious_count = 0;
    double reputation_score = 0.5;
    time_t last_update = 0;
    
    // Serialización para persistencia en LMDB
    std::string serialize() const {
        std::stringstream ss;
        ss << peer_ip << "|" << total_interactions << "|" << suspicious_count << "|"
           << reputation_score << "|" << last_update;
        return ss.str();
    }
};

class NINAMemorySystem {
public:
    static NINAMemorySystem& getInstance();
    
    bool initialize();
    void recordAttackObservation(const std::string& pattern_name, int severity);
    void updatePeerReputation(const std::string& peer_ip, bool is_suspicious);
    
    const std::vector<AttackPattern>& getAttackPatterns() const;
    double getPeerReputation(const std::string& peer_ip) const;
    
    std::string generateSuggestionReport();
    std::string getCurrentAnalysis() const;
    bool applySuggestedImprovement(const std::string& suggestion_id);
    void updatePatternConfidence();
    std::string analyzePatterns() const;
    
    // ============ MÉTODOS DE PERSISTENCIA ============
    // Guardar estado en LMDB (~/.ninacatcoin/lmdb/data.mdb)
    bool persistToLMDB(uint64_t current_height = 0);
    // Cargar estado desde LMDB en startup
    bool loadFromLMDB();
    // Obtener estadísticas de persistencia
    uint64_t getLastPersistHeight() const { return last_persist_height; }
    time_t getLastPersistTime() const { return last_persist_time; }
    
private:
    NINAMemorySystem();
    ~NINAMemorySystem();
    
    std::vector<AttackPattern> attack_patterns;
    std::map<std::string, PeerBehavior> peer_behaviors;
    uint64_t total_observations = 0;
    time_t learning_start_time = 0;
    
    // Tracking de persistencia
    uint64_t last_persist_height = 0;
    time_t last_persist_time = 0;
};

} // namespace ninacatcoin_ai

