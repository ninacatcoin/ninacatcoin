// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <deque>
#include <map>
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * @brief IA that LEARNS from LWMA-1 to improve network stability
 *
 * LWMA-1 (Linear Weighted Moving Average) already exists in:
 *   - /src/cryptonote_basic/difficulty.cpp (line 205+)
 *   - Algorithm: Adjusts mining difficulty based on last 60 blocks
 *   - Target: 120-second block time
 *
 * THIS MODULE (ai_lwma_learning):
 *   1. OBSERVES LWMA-1 behavior
 *   2. LEARNS difficulty patterns
 *   3. DETECTS anomalies (attacks, timestamp manipulation)
 *   4. PREDICTS next difficulty changes
 *   5. ALERTS network of problems
 */
class AILWMALearning {
public:
    /**
     * @brief Snapshot of LWMA-1 state at a block
     */
    struct LWMASnapshot {
        uint64_t block_height;              // Current block height
        uint64_t calculated_difficulty;     // LWMA-1 output
        uint64_t previous_difficulty;       // Previous block difficulty
        uint64_t block_solve_time;          // This block's solve time (seconds)
        double   difficulty_change_percent; // % change from previous
        bool     is_anomalous;              // Detected unusual behavior?
        std::string anomaly_type;           // "rapid_increase", "stalled", "timestamp_attack"
        time_t   timestamp;                 // When this snapshot was recorded
    };

    /**
     * @brief Pattern recognized by the IA
     */
    struct DifficultyPattern {
        std::string pattern_name;           // "stable", "recovering", "attacking"
        uint64_t occurrence_count;          // How many times seen
        double average_block_time;          // Average block solve time
        double stability_score;             // 0-100, higher = more stable
        bool is_concerning;                 // Should trigger alerts?
    };

    /**
     * @brief Initialize LWMA learning
     */
    static void initialize();

    /**
     * @brief Record new block's LWMA-1 calculation
     * @param height Block height
     * @param calculated_difficulty LWMA-1 output
     * @param previous_difficulty Previous difficulty
     * @param block_solve_time Time to solve this block
     */
    static void observe_lwma_calculation(
        uint64_t height,
        uint64_t calculated_difficulty,
        uint64_t previous_difficulty,
        uint64_t block_solve_time
    );

    /**
     * @brief Check if difficulty change is anomalous
     * @param current_diff Current difficulty
     * @param previous_diff Previous difficulty
     * @param block_time Actual block solve time
     * @return Anomaly description or empty string if normal
     */
    static std::string detect_difficulty_anomaly(
        uint64_t current_diff,
        uint64_t previous_diff,
        uint64_t block_time
    );

    /**
     * @brief Recognize current network difficulty pattern
     * @return Pattern description for logging/alerts
     */
    static std::string identify_current_pattern();

    /**
     * @brief Predict next difficulty with confidence level
     * @param recent_blocks Number of recent blocks to analyze
     * @return Predicted difficulty and confidence (e.g., "1000000 (87% confidence)")
     */
    static std::string predict_next_difficulty(int recent_blocks = 10);

    /**
     * @brief Estimate network hashrate from difficulty and block time
     * @return Estimated hashrate as string
     */
    static std::string estimate_network_hashrate();

    /**
     * @brief Check for timestamp manipulation attacks
     * @param recent_times Vector of recent block solve times
     * @return Alert message or empty if no attack detected
     */
    static std::string detect_timestamp_attack(const std::vector<uint64_t>& recent_times);

    /**
     * @brief Generate comprehensive LWMA learning report
     * @return Multi-line report of network health
     */
    static std::string generate_network_health_report();

    /**
     * @brief Get IA's confidence in network stability
     * @return Confidence score 0-100
     */
    static uint8_t get_stability_confidence();

    /**
     * @brief Check if network is in recovery mode
     * @return true if currently recovering from hashrate loss
     */
    static bool is_network_recovering();

    /**
     * @brief Get the current learned patterns
     * @return Map of pattern names to their statistics
     */
    static std::map<std::string, DifficultyPattern> get_learned_patterns();

    /**
     * @brief Reset learning (for network-wide reset events)
     */
    static void reset_learning();

    /**
     * @brief Get statistics for logging
     * @return Formatted statistics string
     */
    static std::string get_statistics();
};

} // namespace ninacatcoin_ai
