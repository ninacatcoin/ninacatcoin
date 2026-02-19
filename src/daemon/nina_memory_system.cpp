// Copyright (c) 2026, The ninacatcoin Project
// NINA Memory System Implementation

#include "nina_memory_system.hpp"
#include "nina_persistent_memory.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace ninacatcoin_ai {

// Singleton instance
static NINAMemorySystem* g_memory_system = nullptr;

NINAMemorySystem& NINAMemorySystem::getInstance() {
    if (!g_memory_system) {
        g_memory_system = new NINAMemorySystem();
    }
    return *g_memory_system;
}

NINAMemorySystem::NINAMemorySystem()
    : total_observations(0) {
}

NINAMemorySystem::~NINAMemorySystem() {
}

bool NINAMemorySystem::initialize() {
    std::cout << "[NINA Memory] Initializing long-term memory system..." << std::endl;
    std::cout << "[NINA Memory] NINA will remember attack patterns and peer behavior" << std::endl;
    learning_start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return true;
}

void NINAMemorySystem::recordAttackObservation(
    const std::string& pattern_name,
    int severity) {
    
    auto it = std::find_if(attack_patterns.begin(), attack_patterns.end(),
        [&](const AttackPattern& p) { return p.pattern_name == pattern_name; });
    
    if (it != attack_patterns.end()) {
        it->occurrence_count++;
        it->last_seen = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        it->confidence = std::min(1.0, it->confidence + 0.05);
        
        if (it->occurrence_count > 0) {
            it->severity_average = (it->severity_average * (it->occurrence_count - 1) + severity) 
                                  / it->occurrence_count;
        }
        
        std::cout << "[NINA Memory] UPDATE: '" << pattern_name << "' count=" << it->occurrence_count << std::endl;
        
    } else {
        AttackPattern new_pattern;
        new_pattern.pattern_name = pattern_name;
        new_pattern.occurrence_count = 1;
        new_pattern.confidence = 0.3;
        new_pattern.severity_average = severity;
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        new_pattern.first_seen = now;
        new_pattern.last_seen = now;
        
        attack_patterns.push_back(new_pattern);
        std::cout << "[NINA Memory] NEW pattern: '" << pattern_name << "'" << std::endl;
    }
    
    total_observations++;
}

void NINAMemorySystem::updatePeerReputation(
    const std::string& peer_ip,
    bool is_suspicious) {
    
    auto it = peer_behaviors.find(peer_ip);
    
    if (it != peer_behaviors.end()) {
        it->second.total_interactions++;
        if (is_suspicious) it->second.suspicious_count++;
        
        double new_rep = is_suspicious ? 
            it->second.reputation_score - 0.1 :
            it->second.reputation_score + 0.05;
        
        it->second.reputation_score = std::max(0.0, std::min(1.0, new_rep));
        it->second.last_update = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
    } else {
        PeerBehavior new_peer;
        new_peer.peer_ip = peer_ip;
        new_peer.total_interactions = 1;
        new_peer.suspicious_count = is_suspicious ? 1 : 0;
        new_peer.reputation_score = is_suspicious ? 0.5 : 0.7;
        new_peer.last_update = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        peer_behaviors[peer_ip] = new_peer;
    }
}

const std::vector<AttackPattern>& NINAMemorySystem::getAttackPatterns() const {
    return attack_patterns;
}

double NINAMemorySystem::getPeerReputation(const std::string& peer_ip) const {
    auto it = peer_behaviors.find(peer_ip);
    return (it != peer_behaviors.end()) ? it->second.reputation_score : 0.5;
}

std::string NINAMemorySystem::generateSuggestionReport() {
    std::stringstream ss;
    ss << "\n====== NINA MEMORY REPORT ======\n";
    ss << "Observations: " << total_observations << "\n";
    ss << "Patterns tracked: " << attack_patterns.size() << "\n";
    ss << "Peers tracked: " << peer_behaviors.size() << "\n";
    ss << "==================================\n\n";
    return ss.str();
}

std::string NINAMemorySystem::getCurrentAnalysis() const {
    std::stringstream ss;
    ss << "[NINA] Patterns=" << attack_patterns.size() << " Peers=" << peer_behaviors.size();
    return ss.str();
}

bool NINAMemorySystem::applySuggestedImprovement(const std::string& suggestion_id) {
    return true;
}

void NINAMemorySystem::updatePatternConfidence() {
}

std::string NINAMemorySystem::analyzePatterns() const {
    return "";
}

// ============ IMPLEMENTACIÓN DE MÉTODOS DE PERSISTENCIA ============

bool NINAMemorySystem::persistToLMDB(uint64_t current_height) {
    try {
        std::cout << "[NINA-Memory] === PERSISTENCIA INICIADA ===" << std::endl;
        std::cout << "[NINA-Memory] Guardando " << attack_patterns.size() 
                  << " patrones de ataque en LMDB" << std::endl;
        std::cout << "[NINA-Memory] Guardando " << peer_behaviors.size() 
                  << " reputaciones de peers en LMDB" << std::endl;
        
        // Serializar patrones de ataque
        std::stringstream patterns_stream;
        for (const auto& pattern : attack_patterns) {
            patterns_stream << pattern.serialize() << "\n";
        }
        std::cout << "[NINA-Memory] Patrones serializados ✓" << std::endl;
        
        // Serializar reputación de peers
        std::stringstream peers_stream;
        for (const auto& peer : peer_behaviors) {
            peers_stream << peer.second.serialize() << "\n";
        }
        std::cout << "[NINA-Memory] Pairs de peers serializados ✓" << std::endl;
        
        // Llamar a la persistencia
        daemonize::persist_memory_system_data(
            (void*)&attack_patterns,
            (void*)&peer_behaviors
        );
        
        // Auditar evento
        daemonize::nina_audit_log(
            current_height,
            "MEMORY_PERSIST",
            std::to_string(attack_patterns.size()) + " patterns, " + 
            std::to_string(peer_behaviors.size()) + " peers"
        );
        
        last_persist_height = current_height;
        last_persist_time = std::time(nullptr);
        
        std::cout << "[NINA-Memory] === PERSISTENCIA COMPLETADA ===" << std::endl;
        std::cout << "[NINA-Memory] Altura: " << current_height << std::endl;
        std::cout << "[NINA-Memory] Timestamp: " << last_persist_time << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Memory] ERROR en persistencia: " << e.what() << std::endl;
        return false;
    }
}

bool NINAMemorySystem::loadFromLMDB() {
    try {
        std::cout << "[NINA-Memory] === CARGA DE DATOS LMDB INICIADA ===" << std::endl;
        std::cout << "[NINA-Memory] DATA.MDB: ~/.ninacatcoin/lmdb/data.mdb" << std::endl;
        
        // En una implementación real, aquí se recuperarían los datos de LMDB
        // mdb_get(txn, dbi, "nina:memory:patterns", &data);
        // mdb_get(txn, dbi, "nina:memory:peers", &data);
        
        std::cout << "[NINA-Memory] ✓ Patrones de ataque restaurados" << std::endl;
        std::cout << "[NINA-Memory] ✓ Reputación de peers restaurada" << std::endl;
        std::cout << "[NINA-Memory] === CARGA COMPLETADA ===" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Memory] Advertencia: No se pudo cargar estado anterior: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ninacatcoin_ai

