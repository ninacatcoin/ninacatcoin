// Copyright (c) 2026, The ninacatcoin Project
// NINA Parameter Adjustor - Dynamic network parameter tuning for FASE 3

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <ctime>

namespace ninacatcoin_ai {

struct ConsensusParameters {
    // Default values
    uint32_t quorum_percentage = 51;        // 51% to 75%
    uint32_t block_time_seconds = 120;      // 60s to 180s
    uint32_t difficulty_multiplier = 100;   // 100 to 200 (percentage)
    uint32_t transaction_pool_size = 10000; // max pending transactions
    uint32_t block_size_limit = 1000000;    // bytes
    double peer_timeout_seconds = 30.0;     // connection timeout
    
    time_t timestamp = 0;
    std::string adjustment_reason = "";
};

enum class AdjustmentType {
    QUORUM_INCREASE,
    QUORUM_DECREASE,
    BLOCK_TIME_INCREASE,
    BLOCK_TIME_DECREASE,
    DIFFICULTY_INCREASE,
    DIFFICULTY_DECREASE,
    POOL_SIZE_INCREASE,
    POOL_SIZE_DECREASE,
    BLOCK_SIZE_ADJUSTMENT,
    PEER_TIMEOUT_ADJUSTMENT
};

struct ParameterAdjustment {
    AdjustmentType type;
    uint32_t old_value;
    uint32_t new_value;
    double confidence = 0.0;
    std::string reason = "";
    time_t timestamp = 0;
};

class NINAParameterAdjustor {
public:
    static NINAParameterAdjustor& getInstance();
    
    bool initialize();
    
    // Get current parameters
    const ConsensusParameters& getCurrentParameters() const;
    ConsensusParameters getOptimalParameters(double network_health_score);
    
    // Propose adjustments
    bool proposeQuorumAdjustment(int new_quorum, const std::string& reason);
    bool proposeBlockTimeAdjustment(int new_seconds, const std::string& reason);
    bool proposeDifficultyAdjustment(int multiplier, const std::string& reason);
    bool proposeTransactionPoolAdjustment(int new_size, const std::string& reason);
    bool proposeBlockSizeAdjustment(int new_size, const std::string& reason);
    
    // Validate and apply adjustments
    bool validateAdjustment(const ParameterAdjustment& adjustment);
    bool applyAdjustment(const ParameterAdjustment& adjustment);
    
    // Get history
    const std::vector<ParameterAdjustment>& getAdjustmentHistory() const;
    const std::vector<ConsensusParameters>& getParameterHistory() const;
    
    std::string getAdjustmentReport() const;
    
    // Safety boundaries
    void setSafetyBoundaries(
        int min_quorum, int max_quorum,
        int min_block_time, int max_block_time,
        int min_difficulty, int max_difficulty
    );
    
private:
    NINAParameterAdjustor();
    ~NINAParameterAdjustor();
    
    bool isAdjustmentWithinBoundaries(const ParameterAdjustment& adj);
    void recordAdjustment(const ParameterAdjustment& adj);
    
    ConsensusParameters current_parameters;
    std::vector<ParameterAdjustment> adjustment_history;
    std::vector<ConsensusParameters> parameter_history;
    
    // Safety boundaries
    int min_quorum = 50, max_quorum = 75;
    int min_block_time = 60, max_block_time = 180;
    int min_difficulty = 50, max_difficulty = 200;
};

} // namespace ninacatcoin_ai
