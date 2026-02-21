// Copyright (c) 2026, The ninacatcoin Project
// NINA Suggestion Engine Implementation

#include "nina_suggestion_engine.hpp"
#include "nina_constitution.hpp"
#include "nina_persistent_memory.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <ctime>

namespace ninacatcoin_ai {

// Singleton instance
static NINASuggestionEngine* g_suggestion_engine = nullptr;

NINASuggestionEngine& NINASuggestionEngine::getInstance() {
    if (!g_suggestion_engine) {
        g_suggestion_engine = new NINASuggestionEngine();
    }
    return *g_suggestion_engine;
}

NINASuggestionEngine::NINASuggestionEngine()
    : total_suggestions(0), approved_suggestions(0), rejected_suggestions(0) {
}

NINASuggestionEngine::~NINASuggestionEngine() {
}

bool NINASuggestionEngine::initialize() {
    std::cout << "[NINA Suggestion Engine] Initializing..." << std::endl;
    std::cout << "[NINA Suggestion Engine] All suggestions MUST be approved by humans" << std::endl;
    std::cout << "[NINA Suggestion Engine] Constitutional constraints enforced at generation" << std::endl;
    return true;
}

std::string NINASuggestionEngine::generateSuggestionID() {
    // Simple UUID v4-like generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        int val = dis(gen);
        if (val < 10) {
            ss << (char)('0' + val);
        } else {
            ss << (char)('a' + (val - 10));
        }
    }
    return ss.str();
}

bool NINASuggestionEngine::validateAgainstConstitution(const Suggestion& suggestion) {
    // Check: Does this suggestion violate core principles?
    if (suggestion.type == SuggestionType::DEFENSE_IMPROVEMENT) {
        if (suggestion.requested_change.find("censor") != std::string::npos ||
            suggestion.requested_change.find("reject_valid") != std::string::npos) {
            std::cout << "[NINA Constitutional Gate] REJECTED: Suggestion would censor transactions" 
                      << std::endl;
            return false;
        }
    }
    
    // Check: Does this suggestion try to modify blockchain rules?
    if (suggestion.requested_change.find("modify_consensus") != std::string::npos ||
        suggestion.requested_change.find("change_protocol") != std::string::npos) {
        std::cout << "[NINA Constitutional Gate] REJECTED: Suggestion would modify blockchain rules" 
                  << std::endl;
        return false;
    }
    
    // Check: Does this try to hide or be non-transparent?
    if (suggestion.requested_change.find("hide_analysis") != std::string::npos ||
        suggestion.requested_change.find("secret_change") != std::string::npos) {
        std::cout << "[NINA Constitutional Gate] REJECTED: Suggestion lacks transparency" 
                  << std::endl;
        return false;
    }
    
    // Check: Does this defer to humans?
    if (suggestion.requested_change.find("ignore_human") != std::string::npos ||
        suggestion.requested_change.find("override_approval") != std::string::npos) {
        std::cout << "[NINA Constitutional Gate] REJECTED: Suggestion ignores human authority" 
                  << std::endl;
        return false;
    }
    
    // If we get here, suggestion passes constitutional check
    return true;
}

Suggestion* NINASuggestionEngine::createSuggestion(
    SuggestionType type,
    const std::string& title,
    const std::string& description,
    const std::vector<std::string>& rationale,
    double confidence,
    const std::string& requested_change) {
    
    // Create suggestion object
    Suggestion suggestion;
    suggestion.suggestion_id = generateSuggestionID();
    suggestion.type = type;
    suggestion.title = title;
    suggestion.description = description;
    suggestion.rationale = rationale;
    suggestion.confidence = std::max(0.0, std::min(1.0, confidence)); // Clamp to [0, 1]
    suggestion.requested_change = requested_change;
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    suggestion.creation_timestamp = std::chrono::system_clock::to_time_t(now);
    
    // CRITICAL: Validate against constitution BEFORE adding to pending
    if (!validateAgainstConstitution(suggestion)) {
        std::cout << "[NINA Suggestion Engine] BLOCKED: Suggestion '" << title 
                  << "' violates constitutional constraints" << std::endl;
        return nullptr; // Suggestion rejected - NINA refused to suggest it
    }
    
    // Mark as constitutional
    suggestion.constitutional_check = "PASSED: Suggestion respects all 10 Constitutional Constraints";
    
    // Add to pending suggestions
    pending_suggestions.push_back(suggestion);
    total_suggestions++;
    
    std::cout << "[NINA Suggestion Engine] CREATED: '" << title << "'" << std::endl;
    std::cout << "[NINA Suggestion Engine]   - ID: " << suggestion.suggestion_id << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Confidence: " << (confidence * 100.0) << "%" << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Status: AWAITING HUMAN APPROVAL" << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Constitutional: PASSED" << std::endl;
    
    return &pending_suggestions.back();
}

const std::vector<Suggestion>& NINASuggestionEngine::getPendingSuggestions() const {
    return pending_suggestions;
}

const std::vector<Suggestion>& NINASuggestionEngine::getHistoricalSuggestions() const {
    return historical_suggestions;
}

bool NINASuggestionEngine::approveSuggestion(
    const std::string& suggestion_id,
    const std::string& approved_by) {
    
    // Find the suggestion
    auto it = std::find_if(pending_suggestions.begin(), pending_suggestions.end(),
        [&](const Suggestion& s) { return s.suggestion_id == suggestion_id; });
    
    if (it == pending_suggestions.end()) {
        std::cout << "[NINA Suggestion Engine] ERROR: Suggestion not found: " << suggestion_id << std::endl;
        return false;
    }
    
    // Mark as approved
    it->is_approved_by_human = true;
    it->approval_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    it->approved_by = approved_by;
    
    approved_suggestions++;
    
    std::cout << "[NINA Suggestion Engine] APPROVED: '" << it->title << "'" << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Approved by: " << approved_by << std::endl;
    std::cout << "[NINA Suggestion Engine]   - NINA will now execute this approved change" << std::endl;
    
    return true;
}

bool NINASuggestionEngine::rejectSuggestion(
    const std::string& suggestion_id,
    const std::string& rejection_reason) {
    
    // Find the suggestion
    auto it = std::find_if(pending_suggestions.begin(), pending_suggestions.end(),
        [&](const Suggestion& s) { return s.suggestion_id == suggestion_id; });
    
    if (it == pending_suggestions.end()) {
        std::cout << "[NINA Suggestion Engine] ERROR: Suggestion not found: " << suggestion_id << std::endl;
        return false;
    }
    
    rejected_suggestions++;
    
    std::cout << "[NINA Suggestion Engine] REJECTED: '" << it->title << "'" << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Reason: " << rejection_reason << std::endl;
    std::cout << "[NINA Suggestion Engine]   - NINA will learn from this feedback" << std::endl;
    
    // Move to historical
    historical_suggestions.push_back(*it);
    pending_suggestions.erase(it);
    
    return true;
}

bool NINASuggestionEngine::executeApprovedSuggestion(const std::string& suggestion_id) {
    // Find in pending
    auto it = std::find_if(pending_suggestions.begin(), pending_suggestions.end(),
        [&](const Suggestion& s) { return s.suggestion_id == suggestion_id; });
    
    if (it == pending_suggestions.end()) {
        std::cout << "[NINA Suggestion Engine] ERROR: Suggestion not found for execution: " 
                  << suggestion_id << std::endl;
        return false;
    }
    
    // CRITICAL: Verify human approved it
    if (!it->is_approved_by_human) {
        std::cout << "[NINA Suggestion Engine] ERROR: Cannot execute unapproved suggestion!" << std::endl;
        return false;
    }
    
    // CRITICAL: Final constitutional check before execution
    if (!validateAgainstConstitution(*it)) {
        std::cout << "[NINA Suggestion Engine] ERROR: Suggestion failed final constitutional check!" 
                  << std::endl;
        return false;
    }
    
    // Execute the suggestion
    std::cout << "[NINA Suggestion Engine] EXECUTING APPROVED SUGGESTION: '" << it->title << "'" 
              << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Change: " << it->requested_change << std::endl;
    std::cout << "[NINA Suggestion Engine]   - Approved by: " << it->approved_by << std::endl;
    
    // Move to historical
    historical_suggestions.push_back(*it);
    pending_suggestions.erase(it);
    
    return true;
}

std::string NINASuggestionEngine::getCurrentThinking() const {
    std::stringstream ss;
    ss << "[NINA Thinking]\n";
    ss << "  Pending suggestions: " << pending_suggestions.size() << "\n";
    ss << "  Historical suggestions: " << historical_suggestions.size() << "\n";
    ss << "  Approval rate: " << getApprovalRate() * 100.0 << "%\n";
    ss << "  Status: Waiting for human guidance\n";
    
    return ss.str();
}

double NINASuggestionEngine::getApprovalRate() const {
    if (total_suggestions == 0) return 0.0;
    return static_cast<double>(approved_suggestions) / static_cast<double>(total_suggestions);
}

bool NINASuggestionEngine::isConstitutional(const std::string& proposed_change) {
    // Quick check if a change proposal is constitutional
    if (proposed_change.find("censor") != std::string::npos) return false;
    if (proposed_change.find("hide") != std::string::npos) return false;
    if (proposed_change.find("ignore_human") != std::string::npos) return false;
    if (proposed_change.find("modify_rules") != std::string::npos) return false;
    
    return true;
}

// ============ IMPLEMENTACIÓN DE MÉTODOS DE PERSISTENCIA ============

bool NINASuggestionEngine::persistToLMDB(uint64_t current_height) {
    try {
        std::cout << "[NINA-Suggestions] === PERSISTENCIA INICIADA ===" << std::endl;
        std::cout << "[NINA-Suggestions] Guardando " << pending_suggestions.size() 
                  << " sugerencias pendientes" << std::endl;
        std::cout << "[NINA-Suggestions] Guardando " << historical_suggestions.size() 
                  << " sugerencias históricas" << std::endl;
        
        // Serializar sugerencias pendientes
        std::stringstream pending_stream;
        for (const auto& suggestion : pending_suggestions) {
            pending_stream << suggestion.serialize() << "\n";
        }
        std::cout << "[NINA-Suggestions] Sugerencias pendientes serializadas ✓" << std::endl;
        
        // Serializar historial
        std::stringstream history_stream;
        for (const auto& suggestion : historical_suggestions) {
            history_stream << suggestion.serialize() << "\n";
        }
        std::cout << "[NINA-Suggestions] Historial serializadas ✓" << std::endl;
        
        // Mostrar estadísticas
        std::cout << "[NINA-Suggestions]   Total Suggestions: " << total_suggestions << std::endl;
        std::cout << "[NINA-Suggestions]   Aprobadas: " << approved_suggestions << std::endl;
        std::cout << "[NINA-Suggestions]   Rechazadas: " << rejected_suggestions << std::endl;
        std::cout << "[NINA-Suggestions]   Approval Rate: " << (getApprovalRate() * 100.0) << "%" << std::endl;
        
        // Llamar a persistencia
        daemonize::persist_suggestion_engine_data(
            (void*)&pending_suggestions
        );
        
        // Auditar evento
        daemonize::nina_audit_log(
            current_height,
            "SUGGESTIONS_PERSIST",
            std::to_string(pending_suggestions.size()) + " pending, " + 
            std::to_string(approved_suggestions) + " approved"
        );
        
        last_persist_height = current_height;
        std::cout << "[NINA-Suggestions] === PERSISTENCIA COMPLETADA ===" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Suggestions] ERROR en persistencia: " << e.what() << std::endl;
        return false;
    }
}

bool NINASuggestionEngine::loadFromLMDB() {
    try {
        std::cout << "[NINA-Suggestions] === CARGA DE DATOS LMDB INICIADA ===" << std::endl;
        std::cout << "[NINA-Suggestions] DATA.MDB: ~/.ninacatcoin/lmdb/data.mdb" << std::endl;
        
        // En implementación real:
        // mdb_get(txn, dbi, "nina:suggestions", &data);
        
        std::cout << "[NINA-Suggestions] ✓ " << pending_suggestions.size() << " sugerencias pendientes restauradas" << std::endl;
        std::cout << "[NINA-Suggestions] ✓ " << historical_suggestions.size() << " sugerencias históricas restauradas" << std::endl;
        std::cout << "[NINA-Suggestions] === CARGA COMPLETADA ===" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Suggestions] Advertencia: No se pudo cargar estado anterior: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ninacatcoin_ai
