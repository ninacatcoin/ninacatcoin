// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.

#include "ai_checkpoint_monitor.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * Implementation of IA Checkpoint Monitoring System
 * 
 * This system allows the IA to:
 * 1. Learn about checkpoints as they're loaded
 * 2. Understand checkpoint distribution
 * 3. Detect fork conditions via checkpoint conflicts
 * 4. Verify blocks against checkpoints
 * 5. Optimize checkpoint loading strategy
 */

// Global instance (accessible to daemon)
static AICheckpointMonitor::CheckpointKnowledge g_ia_checkpoint_knowledge;
static bool g_checkpoint_monitor_initialized = false;

/**
 * @brief Initialize the checkpoint monitoring system
 * Called once when daemon starts (after IA initialization)
 */
void ia_checkpoint_monitor_initialize() {
    if (g_checkpoint_monitor_initialized) {
        return;
    }

    g_ia_checkpoint_knowledge = AICheckpointMonitor::initialize_checkpoint_learning();
    g_checkpoint_monitor_initialized = true;

    MINFO("[IA CHECKPOINTS] Checkpoint monitor initialized");
}

/**
 * @brief Register a checkpoint with the IA monitor
 * 
 * Called whenever checkpoints are loaded from any source
 * (compiled, JSON, or DNS)
 */
void ia_register_checkpoint(
    uint64_t height,
    const std::string& hash_hex,
    const std::string& difficulty,
    const std::string& source
) {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    AICheckpointMonitor::ia_learns_checkpoint(
        g_ia_checkpoint_knowledge,
        height,
        hash_hex,
        difficulty,
        source
    );
}

/**
 * @brief Set the network type (mainnet, testnet, stagenet)
 */
void ia_set_checkpoint_network(const std::string& network_type) {
    g_ia_checkpoint_knowledge.network_type = network_type;
}

/**
 * @brief Get the current checkpoint knowledge
 */
AICheckpointMonitor::CheckpointKnowledge ia_get_checkpoint_knowledge() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }
    return g_ia_checkpoint_knowledge;
}

/**
 * @brief Verify a block against known checkpoints
 * Returns true if block is valid (or no checkpoint exists for that height)
 */
bool ia_verify_block_against_checkpoints(uint64_t block_height, const std::string& block_hash) {
    if (!g_checkpoint_monitor_initialized) {
        return true;  // Can't verify without initialized monitor
    }

    return AICheckpointMonitor::ia_verify_block_against_checkpoints(
        g_ia_checkpoint_knowledge,
        block_height,
        block_hash
    );
}

/**
 * @brief Print checkpoint analysis to logs
 */
void ia_print_checkpoint_analysis() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    std::string analysis = AICheckpointMonitor::ia_analyze_checkpoint_distribution(
        g_ia_checkpoint_knowledge
    );
    MINFO(analysis);
}

/**
 * @brief Print checkpoint status report
 */
void ia_print_checkpoint_status() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    AICheckpointMonitor::ia_log_checkpoint_status(g_ia_checkpoint_knowledge);
}

/**
 * @brief Get checkpoint optimization recommendations
 */
std::string ia_get_checkpoint_optimization_recommendations() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    return AICheckpointMonitor::ia_optimize_checkpoint_loading(g_ia_checkpoint_knowledge);
}

/**
 * @brief Get checkpoint verification strategy recommendation
 */
std::string ia_get_verification_strategy() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    return AICheckpointMonitor::ia_recommend_verification_strategy(g_ia_checkpoint_knowledge);
}

/**
 * @brief Detect fork conditions via checkpoint comparison
 */
bool ia_detect_checkpoint_fork(
    const AICheckpointMonitor::CheckpointKnowledge& peer_knowledge,
    uint64_t& conflict_height
) {
    if (!g_checkpoint_monitor_initialized) {
        return false;
    }

    return AICheckpointMonitor::ia_detect_fork_via_checkpoints(
        g_ia_checkpoint_knowledge,
        peer_knowledge,
        conflict_height
    );
}

/**
 * @brief Get the number of monitored checkpoints
 */
uint64_t ia_get_checkpoint_count() {
    if (!g_checkpoint_monitor_initialized) {
        return 0;
    }
    return g_ia_checkpoint_knowledge.total_checkpoints;
}

/**
 * @brief Get the latest checkpoint height known to IA
 */
uint64_t ia_get_latest_checkpoint_height() {
    if (!g_checkpoint_monitor_initialized) {
        return 0;
    }
    return g_ia_checkpoint_knowledge.latest_checkpoint_height;
}

/**
 * @brief Get the earliest checkpoint height known to IA
 */
uint64_t ia_get_earliest_checkpoint_height() {
    if (!g_checkpoint_monitor_initialized) {
        return 0;
    }
    return g_ia_checkpoint_knowledge.earliest_checkpoint_height;
}

/**
 * @brief Print detailed checkpoint information for debugging
 */
void ia_print_checkpoint_details() {
    if (!g_checkpoint_monitor_initialized) {
        ia_checkpoint_monitor_initialize();
    }

    MINFO("");
    MINFO("═══════════════════════════════════════════════════════════════");
    MINFO("[IA CHECKPOINT DETAILS] Complete Checkpoint List");
    MINFO("═══════════════════════════════════════════════════════════════");
    MINFO("");

    if (g_ia_checkpoint_knowledge.checkpoints.empty()) {
        MINFO("No checkpoints loaded");
        return;
    }

    uint32_t count = 0;
    for (const auto& [height, cp] : g_ia_checkpoint_knowledge.checkpoints) {
        count++;
        MINFO("Checkpoint #" << count << ":");
        MINFO("  Height:     " << height);
        MINFO("  Hash:       " << cp.hash);
        MINFO("  Difficulty: " << cp.difficulty);
        MINFO("  Source:     " << cp.source);
        MINFO("  Loaded:     " << cp.loaded_timestamp);
        MINFO("  Verified:   " << cp.verified_count << " nodes");
        MINFO("");
    }

    MINFO("═══════════════════════════════════════════════════════════════");
}

} // namespace ninacatcoin_ai
