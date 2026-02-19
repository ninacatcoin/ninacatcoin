// Copyright (c) 2026, The ninacatcoin Project
// NINA Consensus Tuner Implementation

#include "nina_consensus_tuner.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

namespace ninacatcoin_ai {

static NINAConsensusTuner* g_consensus_tuner = nullptr;

NINAConsensusTuner& NINAConsensusTuner::getInstance() {
    if (!g_consensus_tuner) {
        g_consensus_tuner = new NINAConsensusTuner();
    }
    return *g_consensus_tuner;
}

NINAConsensusTuner::NINAConsensusTuner() {
}

NINAConsensusTuner::~NINAConsensusTuner() {
}

bool NINAConsensusTuner::initialize() {
    std::cout << "[NINA Consensus Tuner] Initializing FASE 3 - Dynamic Consensus Tuning..." << std::endl;
    std::cout << "[NINA Consensus Tuner] Will monitor network health and auto-adjust parameters" << std::endl;
    std::cout << "[NINA Consensus Tuner] Tuning interval: " << tuning_interval_seconds << " seconds" << std::endl;
    std::cout << "[NINA Consensus Tuner] Auto-execute: " << (auto_execute ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "[NINA Consensus Tuner] Conservatism level: " << conservatism_level << "/5" << std::endl;
    
    last_tuning_analysis = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return true;
}

ConsensusTuningDecision NINAConsensusTuner::analyzeAndProposeTuning() {
    // Get current network metrics
    auto& health_monitor = NINANetworkHealthMonitor::getInstance();
    const NetworkMetrics& metrics = health_monitor.getCurrentMetrics();
    
    // Diagnose health
    NetworkCondition condition = health_monitor.diagnoseNetworkHealth();
    
    // Make tuning decision
    ConsensusTuningDecision decision;
    decision.decision_id = "tuning_" + std::to_string(decision_history.size() + 1);
    decision.condition = condition;
    decision.decision_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    decision.confidence_score = health_monitor.getCurrentMetrics().trust_score;
    
    // Propose adjustments based on condition
    decideTuningStrategy(metrics, condition, decision.proposed_adjustments);
    
    std::cout << "[NINA Consensus Tuner] Analysis complete - " << decision.proposed_adjustments.size() 
              << " adjustments proposed" << std::endl;
    
    return decision;
}

bool NINAConsensusTuner::executeTuning(const ConsensusTuningDecision& decision) {
    if (!validateTuningDecision(decision)) {
        std::cout << "[NINA Consensus Tuner] REJECTED: Tuning decision failed validation" << std::endl;
        return false;
    }
    
    auto& adjustor = NINAParameterAdjustor::getInstance();
    
    bool all_applied = true;
    for (const auto& adjustment : decision.proposed_adjustments) {
        if (!adjustor.applyAdjustment(adjustment)) {
            all_applied = false;
        }
    }
    
    if (all_applied && decision.proposed_adjustments.size() > 0) {
        std::cout << "[NINA Consensus Tuner] EXECUTED: All " << decision.proposed_adjustments.size() 
                  << " adjustments applied successfully" << std::endl;
        
        // Record decision as executed
        ConsensusTuningDecision executed = decision;
        executed.was_executed = true;
        last_decision = executed;
        decision_history.push_back(executed);
        
        return true;
    }
    
    return false;
}

void NINAConsensusTuner::monitorNetworkHealth() {
    auto& health_monitor = NINANetworkHealthMonitor::getInstance();
    
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if ((now - last_tuning_analysis) >= tuning_interval_seconds) {
        adjustParametersIfNeeded();
        last_tuning_analysis = now;
    }
}

void NINAConsensusTuner::adjustParametersIfNeeded() {
    ConsensusTuningDecision decision = analyzeAndProposeTuning();
    
    if (decision.proposed_adjustments.size() > 0) {
        if (auto_execute) {
            executeTuning(decision);
        } else {
            std::cout << "[NINA Consensus Tuner] Proposals ready for review: " 
                      << decision.proposed_adjustments.size() << " adjustments" << std::endl;
        }
    }
}

ConsensusTuningDecision NINAConsensusTuner::makeTuningDecision() {
    return analyzeAndProposeTuning();
}

bool NINAConsensusTuner::validateTuningDecision(const ConsensusTuningDecision& decision) {
    // Ensure not too many adjustments at once (prevents cascade failures)
    if (decision.proposed_adjustments.size() > 5) {
        std::cout << "[NINA Consensus Tuner] VALIDATION FAILED: Too many adjustments proposed" << std::endl;
        return false;
    }
    
    // Ensure confidence is reasonable
    if (decision.confidence_score < 0.3 && decision.proposed_adjustments.size() > 0) {
        std::cout << "[NINA Consensus Tuner] VALIDATION FAILED: Low confidence in decision" << std::endl;
        return false;
    }
    
    return true;
}

const ConsensusTuningDecision& NINAConsensusTuner::getLastDecision() const {
    return last_decision;
}

const std::vector<ConsensusTuningDecision>& NINAConsensusTuner::getDecisionHistory() const {
    return decision_history;
}

void NINAConsensusTuner::setTuningInterval(uint32_t seconds) {
    tuning_interval_seconds = seconds;
}

void NINAConsensusTuner::setAutoExecute(bool enabled) {
    auto_execute = enabled;
    std::cout << "[NINA Consensus Tuner] Auto-execute: " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
}

void NINAConsensusTuner::setConservatismLevel(int level) {
    conservatism_level = std::max(1, std::min(5, level));
    std::cout << "[NINA Consensus Tuner] Conservatism level set to: " << conservatism_level << "/5" << std::endl;
}

std::string NINAConsensusTuner::getTuningReport() const {
    std::stringstream ss;
    ss << "\n====== NINA CONSENSUS TUNING REPORT (FASE 3) ======\n";
    ss << "Total Tuning Decisions: " << decision_history.size() << "\n";
    ss << "Auto-Execute: " << (auto_execute ? "ENABLED" : "DISABLED") << "\n";
    ss << "Conservatism Level: " << conservatism_level << "/5\n";
    ss << "Tuning Interval: " << tuning_interval_seconds << "s\n";
    
    if (!last_decision.proposed_adjustments.empty()) {
        ss << "\nLast Decision:\n";
        ss << "  Condition: ";
        switch (last_decision.condition) {
            case NetworkCondition::OPTIMAL:  ss << "OPTIMAL"; break;
            case NetworkCondition::DEGRADED: ss << "DEGRADED"; break;
            case NetworkCondition::STRESSED: ss << "STRESSED"; break;
            case NetworkCondition::CRITICAL: ss << "CRITICAL"; break;
        }
        ss << "\n  Adjustments: " << last_decision.proposed_adjustments.size() << "\n";
        ss << "  Executed: " << (last_decision.was_executed ? "YES" : "NO") << "\n";
    }
    
    ss << "====================================================\n\n";
    return ss.str();
}

std::string NINAConsensusTuner::getNetworkAdaptationStatus() const {
    std::stringstream ss;
    ss << "[NINA Consensus Tuner] Network Adaptation Status:\n";
    ss << "  Decision History: " << decision_history.size() << " decisions\n";
    ss << "  Last Decision Execute: " << (last_decision.was_executed ? "SUCCESS" : "PENDING") << "\n";
    ss << "  Network Condition: ";
    
    switch (last_decision.condition) {
        case NetworkCondition::OPTIMAL:  ss << "OPTIMAL (no adjustments needed)"; break;
        case NetworkCondition::DEGRADED: ss << "DEGRADED (monitoring)"; break;
        case NetworkCondition::STRESSED: ss << "STRESSED (adjusting)"; break;
        case NetworkCondition::CRITICAL: ss << "CRITICAL (emergency mode)"; break;
    }
    
    ss << "\n";
    return ss.str();
}

void NINAConsensusTuner::decideTuningStrategy(
    const NetworkMetrics& metrics,
    NetworkCondition condition,
    std::vector<ParameterAdjustment>& proposed_adjustments) {
    
    auto& adjustor = NINAParameterAdjustor::getInstance();
    const auto& current_params = adjustor.getCurrentParameters();
    
    std::cout << "[NINA Consensus Tuner] Decision Strategy - Condition: ";
    switch (condition) {
        case NetworkCondition::OPTIMAL:
            std::cout << "OPTIMAL\n";
            // No adjustments needed
            break;
            
        case NetworkCondition::DEGRADED:
            std::cout << "DEGRADED - Light adjustments\n";
            // Slightly increase quorum for safety
            if (current_params.quorum_percentage < 60) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::QUORUM_INCREASE;
                adj.old_value = current_params.quorum_percentage;
                adj.new_value = current_params.quorum_percentage + 3;
                adj.reason = "Network degraded - increasing safety";
                adj.confidence = 0.75;
                proposed_adjustments.push_back(adj);
            }
            break;
            
        case NetworkCondition::STRESSED:
            std::cout << "STRESSED - Moderate adjustments\n";
            
            // Increase quorum
            if (current_params.quorum_percentage < 65) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::QUORUM_INCREASE;
                adj.old_value = current_params.quorum_percentage;
                adj.new_value = std::min(65u, current_params.quorum_percentage + 8);
                adj.reason = "Network stressed - increasing quorum";
                adj.confidence = 0.80;
                proposed_adjustments.push_back(adj);
            }
            
            // Reduce block time to get more blocks faster
            if (current_params.block_time_seconds > 80) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::BLOCK_TIME_DECREASE;
                adj.old_value = current_params.block_time_seconds;
                adj.new_value = std::max(60u, current_params.block_time_seconds - 20);
                adj.reason = "Network stressed - faster blocks for confirmation";
                adj.confidence = 0.70;
                proposed_adjustments.push_back(adj);
            }
            
            // Increase difficulty if hash rate allows
            if (current_params.difficulty_multiplier < 150) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::DIFFICULTY_INCREASE;
                adj.old_value = current_params.difficulty_multiplier;
                adj.new_value = std::min(150u, current_params.difficulty_multiplier + 15);
                adj.reason = "Network stressed - increasing difficulty for security";
                adj.confidence = 0.65;
                proposed_adjustments.push_back(adj);
            }
            break;
            
        case NetworkCondition::CRITICAL:
            std::cout << "CRITICAL - Emergency adjustments\n";
            
            // Maximum quorum
            if (current_params.quorum_percentage < 75) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::QUORUM_INCREASE;
                adj.old_value = current_params.quorum_percentage;
                adj.new_value = 75;
                adj.reason = "CRITICAL: Maximum security protocol activated";
                adj.confidence = 0.95;
                proposed_adjustments.push_back(adj);
            }
            
            // Fast blocks
            if (current_params.block_time_seconds > 60) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::BLOCK_TIME_DECREASE;
                adj.old_value = current_params.block_time_seconds;
                adj.new_value = 60;
                adj.reason = "CRITICAL: Emergency - reducing block time";
                adj.confidence = 0.90;
                proposed_adjustments.push_back(adj);
            }
            
            // Maximum difficulty
            if (current_params.difficulty_multiplier < 200) {
                ParameterAdjustment adj;
                adj.type = AdjustmentType::DIFFICULTY_INCREASE;
                adj.old_value = current_params.difficulty_multiplier;
                adj.new_value = 200;
                adj.reason = "CRITICAL: Activating maximum difficulty";
                adj.confidence = 0.85;
                proposed_adjustments.push_back(adj);
            }
            break;
    }
}

} // namespace ninacatcoin_ai
