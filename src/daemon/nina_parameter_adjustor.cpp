// Copyright (c) 2026, The ninacatcoin Project
// NINA Parameter Adjustor Implementation

#include "nina_parameter_adjustor.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace ninacatcoin_ai {

static NINAParameterAdjustor* g_adjustor = nullptr;

NINAParameterAdjustor& NINAParameterAdjustor::getInstance() {
    if (!g_adjustor) {
        g_adjustor = new NINAParameterAdjustor();
    }
    return *g_adjustor;
}

NINAParameterAdjustor::NINAParameterAdjustor() {
    // Initialize with default parameters
    current_parameters.quorum_percentage = 51;
    current_parameters.block_time_seconds = 120;
    current_parameters.difficulty_multiplier = 100;
    current_parameters.transaction_pool_size = 10000;
    current_parameters.block_size_limit = 1000000;
    current_parameters.peer_timeout_seconds = 30.0;
    current_parameters.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

NINAParameterAdjustor::~NINAParameterAdjustor() {
}

bool NINAParameterAdjustor::initialize() {
    std::cout << "[NINA Parameter Adjustor] Initializing dynamic parameter tuning..." << std::endl;
    std::cout << "[NINA Parameter Adjustor] Initial Quorum: " << current_parameters.quorum_percentage << "%" << std::endl;
    std::cout << "[NINA Parameter Adjustor] Initial Block Time: " << current_parameters.block_time_seconds << "s" << std::endl;
    
    parameter_history.push_back(current_parameters);
    return true;
}

const ConsensusParameters& NINAParameterAdjustor::getCurrentParameters() const {
    return current_parameters;
}

ConsensusParameters NINAParameterAdjustor::getOptimalParameters(double network_health_score) {
    ConsensusParameters optimal = current_parameters;
    
    // Scale adjustments based on network health (0.0 = bad, 1.0 = perfect)
    if (network_health_score < 0.3) {
        // Network in trouble - increase security
        optimal.quorum_percentage = std::min(75u, current_parameters.quorum_percentage + 10);
        optimal.block_time_seconds = std::max(60u, current_parameters.block_time_seconds - 20);
        optimal.difficulty_multiplier = std::min(200u, current_parameters.difficulty_multiplier + 25);
    } else if (network_health_score < 0.6) {
        // Network degraded - moderate adjustments
        optimal.quorum_percentage = std::min(75u, current_parameters.quorum_percentage + 5);
        optimal.block_time_seconds = std::max(60u, current_parameters.block_time_seconds - 10);
        optimal.difficulty_multiplier = std::min(200u, current_parameters.difficulty_multiplier + 10);
    } else if (network_health_score > 0.9) {
        // Network healthy - can relax
        optimal.quorum_percentage = std::max(50u, current_parameters.quorum_percentage - 5);
        optimal.block_time_seconds = std::min(180u, current_parameters.block_time_seconds + 10);
        optimal.difficulty_multiplier = std::max(100u, current_parameters.difficulty_multiplier - 10);
    }
    
    return optimal;
}

bool NINAParameterAdjustor::proposeQuorumAdjustment(int new_quorum, const std::string& reason) {
    ParameterAdjustment adj;
    adj.type = (new_quorum > current_parameters.quorum_percentage) ? 
        AdjustmentType::QUORUM_INCREASE : AdjustmentType::QUORUM_DECREASE;
    adj.old_value = current_parameters.quorum_percentage;
    adj.new_value = new_quorum;
    adj.reason = reason;
    adj.confidence = 0.85;
    adj.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (validateAdjustment(adj)) {
        std::cout << "[NINA Parameter Adjustor] Proposed Quorum: " << current_parameters.quorum_percentage 
                  << "% → " << new_quorum << "% (" << reason << ")" << std::endl;
        return true;
    }
    return false;
}

bool NINAParameterAdjustor::proposeBlockTimeAdjustment(int new_seconds, const std::string& reason) {
    ParameterAdjustment adj;
    adj.type = (new_seconds > current_parameters.block_time_seconds) ? 
        AdjustmentType::BLOCK_TIME_INCREASE : AdjustmentType::BLOCK_TIME_DECREASE;
    adj.old_value = current_parameters.block_time_seconds;
    adj.new_value = new_seconds;
    adj.reason = reason;
    adj.confidence = 0.80;
    adj.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (validateAdjustment(adj)) {
        std::cout << "[NINA Parameter Adjustor] Proposed Block Time: " << current_parameters.block_time_seconds 
                  << "s → " << new_seconds << "s (" << reason << ")" << std::endl;
        return true;
    }
    return false;
}

bool NINAParameterAdjustor::proposeDifficultyAdjustment(int multiplier, const std::string& reason) {
    ParameterAdjustment adj;
    adj.type = (multiplier > current_parameters.difficulty_multiplier) ? 
        AdjustmentType::DIFFICULTY_INCREASE : AdjustmentType::DIFFICULTY_DECREASE;
    adj.old_value = current_parameters.difficulty_multiplier;
    adj.new_value = multiplier;
    adj.reason = reason;
    adj.confidence = 0.75;
    adj.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (validateAdjustment(adj)) {
        std::cout << "[NINA Parameter Adjustor] Proposed Difficulty: " << current_parameters.difficulty_multiplier 
                  << " → " << multiplier << " (" << reason << ")" << std::endl;
        return true;
    }
    return false;
}

bool NINAParameterAdjustor::proposeTransactionPoolAdjustment(int new_size, const std::string& reason) {
    ParameterAdjustment adj;
    adj.type = (new_size > current_parameters.transaction_pool_size) ? 
        AdjustmentType::POOL_SIZE_INCREASE : AdjustmentType::POOL_SIZE_DECREASE;
    adj.old_value = current_parameters.transaction_pool_size;
    adj.new_value = new_size;
    adj.reason = reason;
    adj.confidence = 0.70;
    adj.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (validateAdjustment(adj)) {
        return true;
    }
    return false;
}

bool NINAParameterAdjustor::proposeBlockSizeAdjustment(int new_size, const std::string& reason) {
    ParameterAdjustment adj;
    adj.type = AdjustmentType::BLOCK_SIZE_ADJUSTMENT;
    adj.old_value = current_parameters.block_size_limit;
    adj.new_value = new_size;
    adj.reason = reason;
    adj.confidence = 0.65;
    adj.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (validateAdjustment(adj)) {
        return true;
    }
    return false;
}

bool NINAParameterAdjustor::validateAdjustment(const ParameterAdjustment& adjustment) {
    return isAdjustmentWithinBoundaries(adjustment);
}

bool NINAParameterAdjustor::applyAdjustment(const ParameterAdjustment& adjustment) {
    if (!validateAdjustment(adjustment)) {
        std::cout << "[NINA Parameter Adjustor] REJECTED: Adjustment outside safety boundaries" << std::endl;
        return false;
    }
    
    // Apply the adjustment
    switch (adjustment.type) {
        case AdjustmentType::QUORUM_INCREASE:
        case AdjustmentType::QUORUM_DECREASE:
            current_parameters.quorum_percentage = adjustment.new_value;
            break;
        case AdjustmentType::BLOCK_TIME_INCREASE:
        case AdjustmentType::BLOCK_TIME_DECREASE:
            current_parameters.block_time_seconds = adjustment.new_value;
            break;
        case AdjustmentType::DIFFICULTY_INCREASE:
        case AdjustmentType::DIFFICULTY_DECREASE:
            current_parameters.difficulty_multiplier = adjustment.new_value;
            break;
        case AdjustmentType::POOL_SIZE_INCREASE:
        case AdjustmentType::POOL_SIZE_DECREASE:
            current_parameters.transaction_pool_size = adjustment.new_value;
            break;
        case AdjustmentType::BLOCK_SIZE_ADJUSTMENT:
            current_parameters.block_size_limit = adjustment.new_value;
            break;
        default:
            return false;
    }
    
    current_parameters.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    current_parameters.adjustment_reason = adjustment.reason;
    
    recordAdjustment(adjustment);
    parameter_history.push_back(current_parameters);
    
    std::cout << "[NINA Parameter Adjustor] APPLIED: " << adjustment.reason << std::endl;
    return true;
}

const std::vector<ParameterAdjustment>& NINAParameterAdjustor::getAdjustmentHistory() const {
    return adjustment_history;
}

const std::vector<ConsensusParameters>& NINAParameterAdjustor::getParameterHistory() const {
    return parameter_history;
}

std::string NINAParameterAdjustor::getAdjustmentReport() const {
    std::stringstream ss;
    ss << "\n====== NINA PARAMETER ADJUSTMENT REPORT ======\n";
    ss << "Current Quorum: " << current_parameters.quorum_percentage << "%\n";
    ss << "Current Block Time: " << current_parameters.block_time_seconds << "s\n";
    ss << "Current Difficulty: " << current_parameters.difficulty_multiplier << "\n";
    ss << "Total Adjustments Applied: " << adjustment_history.size() << "\n";
    ss << "=============================================\n\n";
    return ss.str();
}

void NINAParameterAdjustor::setSafetyBoundaries(
    int min_quorum, int max_quorum,
    int min_block_time, int max_block_time,
    int min_difficulty, int max_difficulty) {
    
    this->min_quorum = min_quorum;
    this->max_quorum = max_quorum;
    this->min_block_time = min_block_time;
    this->max_block_time = max_block_time;
    this->min_difficulty = min_difficulty;
    this->max_difficulty = max_difficulty;
}

bool NINAParameterAdjustor::isAdjustmentWithinBoundaries(const ParameterAdjustment& adj) {
    switch (adj.type) {
        case AdjustmentType::QUORUM_INCREASE:
        case AdjustmentType::QUORUM_DECREASE:
            return adj.new_value >= min_quorum && adj.new_value <= max_quorum;
        case AdjustmentType::BLOCK_TIME_INCREASE:
        case AdjustmentType::BLOCK_TIME_DECREASE:
            return adj.new_value >= min_block_time && adj.new_value <= max_block_time;
        case AdjustmentType::DIFFICULTY_INCREASE:
        case AdjustmentType::DIFFICULTY_DECREASE:
            return adj.new_value >= min_difficulty && adj.new_value <= max_difficulty;
        default:
            return true;  // Other adjustments always valid for now
    }
}

void NINAParameterAdjustor::recordAdjustment(const ParameterAdjustment& adj) {
    adjustment_history.push_back(adj);
}

} // namespace ninacatcoin_ai
