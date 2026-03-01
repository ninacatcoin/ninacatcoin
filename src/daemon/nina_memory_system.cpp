// Copyright (c) 2026, The ninacatcoin Project
// NINA Memory System Implementation

#include "nina_memory_system.hpp"
#include "nina_persistent_memory.hpp"
#include "nina_llm_bridge.hpp"
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
    
    // LLM: Enhance memory report with pattern correlation
    try {
        auto& bridge = NinaLLMBridge::getInstance();
        if (bridge.is_available() && !attack_patterns.empty()) {
            std::stringstream patterns_str;
            for (const auto& p : attack_patterns) {
                patterns_str << p.pattern_name << "(count=" << p.occurrence_count
                             << ",conf=" << p.confidence
                             << ",sev=" << p.severity_average << "); ";
            }
            std::stringstream peers_str;
            int suspicious_peers = 0;
            for (const auto& pr : peer_behaviors) {
                if (pr.second.reputation_score < 0.3) suspicious_peers++;
            }
            peers_str << "total=" << peer_behaviors.size()
                      << ",suspicious=" << suspicious_peers;
            
            auto correlation = bridge.correlate_attack_patterns(
                patterns_str.str(), peers_str.str());
            if (!correlation.analysis.empty()) {
                ss << "[NINA LLM Pattern Correlation]\n";
                ss << "  Score: " << correlation.correlation_score << "\n";
                ss << "  Analysis: " << correlation.analysis << "\n";
                if (!correlation.recommended_action.empty()) {
                    ss << "  Action: " << correlation.recommended_action << "\n";
                }
                ss << "\n";
            }
        }
    } catch (...) {}
    
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
    // LLM: Use LLM to correlate attack patterns with peer behavior
    try {
        auto& bridge = NinaLLMBridge::getInstance();
        if (bridge.is_available() && !attack_patterns.empty()) {
            std::stringstream patterns_str;
            for (const auto& p : attack_patterns) {
                patterns_str << p.pattern_name << "(count=" << p.occurrence_count
                             << ",confidence=" << p.confidence
                             << ",severity=" << p.severity_average << "); ";
            }
            std::stringstream peers_str;
            for (const auto& pr : peer_behaviors) {
                if (pr.second.reputation_score < 0.4) {
                    peers_str << pr.first << "(rep=" << pr.second.reputation_score
                              << ",suspicious=" << pr.second.suspicious_count << "); ";
                }
            }
            auto result = bridge.correlate_attack_patterns(
                patterns_str.str(), peers_str.str());
            if (!result.analysis.empty()) {
                return "[LLM] " + result.analysis + " | Action: " + result.recommended_action;
            }
        }
    } catch (...) {}
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
        
        // Llamar a la persistencia con datos ya serializados
        daemonize::persist_memory_system_data(
            patterns_stream.str(),
            peers_stream.str(),
            attack_patterns.size(),
            peer_behaviors.size()
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

        std::string patterns_data, peers_data;
        if (!daemonize::load_memory_system_data(patterns_data, peers_data)) {
            std::cout << "[NINA-Memory] No previous memory data found (first run)" << std::endl;
            return false;
        }

        // Parse attack patterns
        if (!patterns_data.empty()) {
            std::istringstream pstream(patterns_data);
            std::string line;
            size_t loaded = 0;
            while (std::getline(pstream, line)) {
                if (line.empty()) continue;
                // Format: name|count|confidence|severity|first_seen|last_seen
                std::istringstream ls(line);
                AttackPattern pat;
                std::string token;
                if (std::getline(ls, pat.pattern_name, '|') &&
                    std::getline(ls, token, '|')) {
                    pat.occurrence_count = std::stoull(token);
                    if (std::getline(ls, token, '|')) pat.confidence = std::stod(token);
                    if (std::getline(ls, token, '|')) pat.severity_average = std::stod(token);
                    if (std::getline(ls, token, '|')) pat.first_seen = (time_t)std::stoll(token);
                    if (std::getline(ls, token, '|')) pat.last_seen = (time_t)std::stoll(token);
                    attack_patterns.push_back(pat);
                    loaded++;
                }
            }
            std::cout << "[NINA-Memory] ✓ " << loaded << " patrones de ataque restaurados" << std::endl;
        }

        // Parse peer behaviors
        if (!peers_data.empty()) {
            std::istringstream pstream(peers_data);
            std::string line;
            size_t loaded = 0;
            while (std::getline(pstream, line)) {
                if (line.empty()) continue;
                // Format: ip|interactions|suspicious|reputation|last_update
                std::istringstream ls(line);
                PeerBehavior pb;
                std::string token;
                if (std::getline(ls, pb.peer_ip, '|') &&
                    std::getline(ls, token, '|')) {
                    pb.total_interactions = std::stoull(token);
                    if (std::getline(ls, token, '|')) pb.suspicious_count = std::stoull(token);
                    if (std::getline(ls, token, '|')) pb.reputation_score = std::stod(token);
                    if (std::getline(ls, token, '|')) pb.last_update = (time_t)std::stoll(token);
                    peer_behaviors[pb.peer_ip] = pb;
                    loaded++;
                }
            }
            std::cout << "[NINA-Memory] ✓ " << loaded << " reputaciones de peers restauradas" << std::endl;
        }

        std::cout << "[NINA-Memory] === CARGA COMPLETADA ===" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Memory] Advertencia: No se pudo cargar estado anterior: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ninacatcoin_ai

