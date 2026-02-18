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
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <iomanip>
#include "misc_log_ex.h"
#include "crypto/hash.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * @brief IA Checkpoint Monitor - Understands and monitors blockchain checkpoints
 * 
 * The IA module learns how checkpoints work and monitors them to ensure
 * the network is synchronized correctly and checkpoints are valid.
 * 
 * CHECKPOINT BASICS (IA's understanding):
 * =====================================
 * 
 * A checkpoint is a known-good block at a specific height:
 * - Height: Block number (e.g., 100000)
 * - Hash: SHA-256 hash of the block
 * - Difficulty: Cumulative difficulty at that height
 * 
 * Purpose:
 * ✓ Speed up blockchain synchronization
 * ✓ Prevent long-range attacks
 * ✓ Allow quick network forks detection
 * ✓ Provide trusted reference points
 * 
 * Sources:
 * ✓ Compiled in binary (hardcoded)
 * ✓ checkpoints.json file (on disk)
 * ✓ DNS checkpoints (network-fetched)
 */
class AICheckpointMonitor {
public:
    /**
     * @brief Structure for checkpoint data (IA's internal representation)
     */
    struct CheckpointData {
        uint64_t height;                          // Block height
        std::string hash;                         // Block hash (hex string)
        std::string difficulty;                   // Cumulative difficulty
        std::string source;                       // Where loaded from: "compiled", "json", "dns"
        uint64_t loaded_timestamp;                // When IA loaded this checkpoint
        uint32_t verified_count;                  // How many nodes verified it
    };

    /**
     * @brief IA's checkpoint knowledge base
     */
    struct CheckpointKnowledge {
        std::map<uint64_t, CheckpointData> checkpoints;  // height → checkpoint data
        uint64_t earliest_checkpoint_height;             // Oldest checkpoint IA knows about
        uint64_t latest_checkpoint_height;               // Newest checkpoint IA knows about
        uint64_t total_checkpoints;                      // Total checkpoints being monitored
        std::string network_type;                        // mainnet, testnet, stagenet
    };

    /**
     * @brief Initialize IA checkpoint monitor
     */
    static CheckpointKnowledge initialize_checkpoint_learning() {
        CheckpointKnowledge knowledge;
        
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("[IA CHECKPOINTS] Initializing checkpoint monitoring system");
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("");
        MINFO("[IA] Learning checkpoint concept:");
        MINFO("  └─ Checkpoints = Known-good blocks at specific heights");
        MINFO("  └─ Use: Network synchronization & security");
        MINFO("  └─ Sources: Compiled, JSON files, DNS servers");
        MINFO("");
        
        knowledge.earliest_checkpoint_height = 0;
        knowledge.latest_checkpoint_height = 0;
        knowledge.total_checkpoints = 0;
        knowledge.network_type = "unknown";
        
        return knowledge;
    }

    /**
     * @brief IA learns about a new checkpoint
     * 
     * When the daemon loads checkpoints, IA analyzes and learns their patterns.
     */
    static void ia_learns_checkpoint(
        CheckpointKnowledge& knowledge,
        uint64_t height,
        const std::string& hash_hex,
        const std::string& difficulty,
        const std::string& source
    ) {
        CheckpointData cp;
        cp.height = height;
        cp.hash = hash_hex;
        cp.difficulty = difficulty;
        cp.source = source;
        cp.loaded_timestamp = time(nullptr);
        cp.verified_count = 0;

        knowledge.checkpoints[height] = cp;
        
        // Update statistics
        if (knowledge.earliest_checkpoint_height == 0 || height < knowledge.earliest_checkpoint_height) {
            knowledge.earliest_checkpoint_height = height;
        }
        if (height > knowledge.latest_checkpoint_height) {
            knowledge.latest_checkpoint_height = height;
        }
        knowledge.total_checkpoints++;

        MDEBUG("[IA CHECKPOINTS] Learned checkpoint at height " << height 
               << " from " << source << " with hash " << hash_hex.substr(0, 16) << "...");
    }

    /**
     * @brief IA analyzes checkpoint spacing and distribution
     * 
     * Returns insights about checkpoint distribution across the blockchain
     */
    static std::string ia_analyze_checkpoint_distribution(const CheckpointKnowledge& knowledge) {
        if (knowledge.checkpoints.empty()) {
            return "No checkpoints loaded";
        }

        std::ostringstream analysis;
        analysis << "\n";
        analysis << "╔════════════════════════════════════════════════════════════╗\n";
        analysis << "║        IA CHECKPOINT ANALYSIS                              ║\n";
        analysis << "╚════════════════════════════════════════════════════════════╝\n";
        analysis << "\n";
        analysis << "Network Type: " << knowledge.network_type << "\n";
        analysis << "Total Checkpoints: " << knowledge.total_checkpoints << "\n";
        analysis << "Height Range: " << knowledge.earliest_checkpoint_height 
                 << " → " << knowledge.latest_checkpoint_height << "\n";
        
        // Calculate average spacing
        if (knowledge.total_checkpoints > 1) {
            uint64_t height_range = knowledge.latest_checkpoint_height - knowledge.earliest_checkpoint_height;
            uint64_t average_spacing = height_range / (knowledge.total_checkpoints - 1);
            analysis << "Average Checkpoint Spacing: " << average_spacing << " blocks\n";
        }

        analysis << "\nCheckpoint Distribution:\n";
        uint32_t count = 0;
        for (const auto& [height, cp] : knowledge.checkpoints) {
            if (count++ < 5 || count > knowledge.total_checkpoints - 5) {
                analysis << "  Height " << std::setw(8) << height 
                        << ": " << cp.hash.substr(0, 16) << "... (from " << cp.source << ")\n";
            } else if (count == 6) {
                analysis << "  ... " << (knowledge.total_checkpoints - 10) << " more checkpoints ...\n";
            }
        }

        return analysis.str();
    }

    /**
     * @brief IA monitors checkpoint validity
     * 
     * Checks if a new block matches known checkpoints
     */
    static bool ia_verify_block_against_checkpoints(
        const CheckpointKnowledge& knowledge,
        uint64_t block_height,
        const std::string& block_hash
    ) {
        // Check if this height has a checkpoint
        auto it = knowledge.checkpoints.find(block_height);
        if (it == knowledge.checkpoints.end()) {
            // No checkpoint at this height, can't verify
            return true;  // Not a failure, just unverifiable
        }

        const CheckpointData& cp = it->second;
        if (cp.hash == block_hash) {
            // Hash matches checkpoint - GOOD
            MDEBUG("[IA CHECKPOINTS] Block height " << block_height 
                   << " matches checkpoint ✓");
            return true;
        } else {
            // Hash DOES NOT match checkpoint - PROBLEM
            MERROR("[IA CHECKPOINTS] CHECKPOINT MISMATCH at height " << block_height);
            MERROR("  Expected: " << cp.hash);
            MERROR("  Got:      " << block_hash);
            return false;
        }
    }

    /**
     * @brief IA detects network forks via checkpoints
     * 
     * If nodes are conflicting on checkpoints, a fork is happening
     */
    static bool ia_detect_fork_via_checkpoints(
        const CheckpointKnowledge& our_knowledge,
        const CheckpointKnowledge& peer_knowledge,
        uint64_t& conflict_height
    ) {
        // Check for checkpoint conflicts
        for (const auto& [height, our_cp] : our_knowledge.checkpoints) {
            auto peer_it = peer_knowledge.checkpoints.find(height);
            if (peer_it != peer_knowledge.checkpoints.end()) {
                const CheckpointData& peer_cp = peer_it->second;
                
                if (our_cp.hash != peer_cp.hash) {
                    // Conflict detected
                    MERROR("[IA CHECKPOINTS] FORK DETECTED!");
                    MERROR("  Height: " << height);
                    MERROR("  Our hash:   " << our_cp.hash.substr(0, 16) << "...");
                    MERROR("  Peer hash:  " << peer_cp.hash.substr(0, 16) << "...");
                    
                    conflict_height = height;
                    return true;  // Fork detected
                }
            }
        }

        return false;  // No fork
    }

    /**
     * @brief IA recommends checkpoint verification strategy
     * 
     * Based on checkpoint distribution, suggest optimal verification
     */
    static std::string ia_recommend_verification_strategy(const CheckpointKnowledge& knowledge) {
        std::ostringstream recommendation;
        
        recommendation << "\n";
        recommendation << "IA CHECKPOINT VERIFICATION STRATEGY:\n";
        recommendation << "═════════════════════════════════════════════\n";
        recommendation << "\n";

        if (knowledge.total_checkpoints < 10) {
            recommendation << "⚠️  WARNING: Few checkpoints (" << knowledge.total_checkpoints << ")\n";
            recommendation << "   Recommendation: Verify all blocks between checkpoints\n";
            recommendation << "   Risk Level: MEDIUM\n";
        } else if (knowledge.total_checkpoints < 100) {
            recommendation << "✓ Moderate checkpoint coverage\n";
            recommendation << "  Recommendation: Verify ~10% of blocks between checkpoints\n";
            recommendation << "  Risk Level: LOW\n";
        } else {
            recommendation << "✓ Excellent checkpoint coverage\n";
            recommendation << "  Recommendation: Trust checkpoints, spot-check every 10th block\n";
            recommendation << "  Risk Level: VERY LOW\n";
        }

        recommendation << "\n";
        recommendation << "Verification Procedure:\n";
        recommendation << "  1. Download checkpoint manifest\n";
        recommendation << "  2. Verify checkpoints with" << knowledge.total_checkpoints << "+ nodes\n";
        recommendation << "  3. Fetch blocks between checkpoints\n";
        recommendation << "  4. Validate block hashes\n";
        recommendation << "  5. Trust for all blocks beyond latest checkpoint\n";

        return recommendation.str();
    }

    /**
     * @brief IA logs checkpoint monitoring status
     */
    static void ia_log_checkpoint_status(const CheckpointKnowledge& knowledge) {
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("[IA CHECKPOINT MONITOR] Status Report");
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("");
        MINFO("Network: " << knowledge.network_type);
        MINFO("Total Checkpoints Being Monitored: " << knowledge.total_checkpoints);
        MINFO("Height Range: " << knowledge.earliest_checkpoint_height 
              << " → " << knowledge.latest_checkpoint_height);
        MINFO("");
        MINFO("Checkpoint Sources Detected:");
        
        std::map<std::string, uint32_t> source_count;
        for (const auto& [height, cp] : knowledge.checkpoints) {
            source_count[cp.source]++;
        }
        
        for (const auto& [source, count] : source_count) {
            MINFO("  + " << source << ": " << count << " checkpoints");
        }
        
        MINFO("");
        MINFO("IA Checkpoint Monitoring: ACTIVE");
        MINFO("═══════════════════════════════════════════════════════════════");
    }

    /**
     * @brief IA helps optimize checkpoint loading
     * 
     * Suggests which checkpoints to load based on network needs
     */
    static std::string ia_optimize_checkpoint_loading(const CheckpointKnowledge& knowledge) {
        std::ostringstream optimization;
        
        optimization << "\n";
        optimization << "CHECKPOINT LOADING OPTIMIZATION:\n";
        optimization << "════════════════════════════════════════\n";
        optimization << "\n";

        if (knowledge.total_checkpoints == 0) {
            optimization << "⚠️  No checkpoints loaded\n";
            optimization << "Recommendation: Load from JSON first, then DNS\n";
            return optimization.str();
        }

        // Recommend which checkpoints are most important
        optimization << "Most Important Checkpoints (for sync):\n";
        
        // Early, middle, and late checkpoints are most important
        uint32_t count = 0;
        for (const auto& [height, cp] : knowledge.checkpoints) {
            if (count < 3 || count > knowledge.total_checkpoints - 3) {
                optimization << "  Height " << height << ": Critical\n";
            }
            count++;
        }

        optimization << "\nLatest Checkpoint:\n";
        const CheckpointData& latest = knowledge.checkpoints.rbegin()->second;
        optimization << "  Height: " << latest.height << "\n";
        optimization << "  Hash: " << latest.hash.substr(0, 32) << "...\n";
        optimization << "  Source: " << latest.source << "\n";

        return optimization.str();
    }
};

} // namespace ninacatcoin_ai
