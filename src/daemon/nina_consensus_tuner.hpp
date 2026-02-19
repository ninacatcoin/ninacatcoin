// Copyright (c) 2026, The ninacatcoin Project
// NINA Consensus Tuner - Orchestrator for dynamic consensus tuning (FASE 3)

#pragma once

#include "nina_network_health_monitor.hpp"
#include "nina_parameter_adjustor.hpp"
#include <string>
#include <vector>
#include <ctime>

namespace ninacatcoin_ai {

struct ConsensusTuningDecision {
    std::string decision_id;
    NetworkCondition condition;
    double confidence_score;
    std::vector<ParameterAdjustment> proposed_adjustments;
    time_t decision_timestamp;
    std::string rationale;
    bool was_executed = false;
};

class NINAConsensusTuner {
public:
    static NINAConsensusTuner& getInstance();
    
    bool initialize();
    
    // Main tuning loop
    ConsensusTuningDecision analyzeAndProposeTuning();
    bool executeTuning(const ConsensusTuningDecision& decision);
    
    // Continuous monitoring
    void monitorNetworkHealth();
    void adjustParametersIfNeeded();
    
    // Decision making
    ConsensusTuningDecision makeTuningDecision();
    bool validateTuningDecision(const ConsensusTuningDecision& decision);
    
    // Get current state
    const ConsensusTuningDecision& getLastDecision() const;
    const std::vector<ConsensusTuningDecision>& getDecisionHistory() const;
    
    // Configuration
    void setTuningInterval(uint32_t seconds);
    void setAutoExecute(bool enabled);
    void setConservatismLevel(int level);  // 1=aggressive, 5=conservative
    
    // Reporting
    std::string getTuningReport() const;
    std::string getNetworkAdaptationStatus() const;
    
private:
    NINAConsensusTuner();
    ~NINAConsensusTuner();
    
    // Decision logic
    void decideTuningStrategy(
        const NetworkMetrics& metrics,
        NetworkCondition condition,
        std::vector<ParameterAdjustment>& proposed_adjustments
    );
    
    // P2P coordination (not implemented yet, placeholder for FASE 2)
    bool synchronizeWithPeers(const ConsensusTuningDecision& decision);
    
    ConsensusTuningDecision last_decision;
    std::vector<ConsensusTuningDecision> decision_history;
    
    uint32_t tuning_interval_seconds = 60;  // Run analysis every 60s
    bool auto_execute = true;
    int conservatism_level = 3;              // 1-5, default moderate
    
    time_t last_tuning_analysis = 0;
};

} // namespace ninacatcoin_ai
