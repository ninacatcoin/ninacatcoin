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

#include <cstdint>
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <ctime>
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * @brief IA Hashrate Recovery Monitor + Checkpoint Guardian
 * 
 * The IA learns and monitors ninacatcoin's network difficulty adjustment system
 * that enables rapid hashrate recovery when a large miner departs.
 * 
 * ADDITIONALLY: NINA acts as guardian of the checkpoint system, validating that
 * checkpoints ONLY come from the official seed nodes that generate them.
 * 
 * OFFICIAL SEED NODES:
 *   - Seed1 (87.106.7.156): Generates and uploads checkpoints.json every hour
 *   - Seed2 (217.154.196.9): Generates and uploads checkpoints.dat every hour
 * 
 * These are the ONLY authorized sources for valid checkpoints.
 * NINA will REJECT any checkpoints from other sources.
 * 
 * CHECKPOINT STRUCTURE KNOWLEDGE:
 *   - epoch_id: Changes every hour (unique ID for each generation)
 *   - generated_at_ts: Unix timestamp when checkpoint was created
 *   - checkpoint_interval: 30 blocks (JSON checkpoints every 30 blocks)
 *   - hashlines: List of {height, hash} pairs
 *   - Seed1 generates JSON format
 *   - Seed2 generates DAT (binary) format
 * 
 * NINA MONITORS:
 *   - If epoch_id stays same for >2 hours → Seed not generating (ALERT)
 *   - If epoch_id decreases → Attacked or corrupted (CRITICAL)
 *   - If generated_at_ts is stale → Seed offline or frozen (WARNING)
 *   - If two seeds have different epoch_id → Desynchronized (ALERT)
 * 
 * SYSTEM COMPONENTS:
 * ══════════════════════════════════════════════════════════════
 * 
 * 1. LWMA-1 Algorithm (Linear Weighted Moving Average)
 *    └─ Uses last 60 blocks with linear weighting
 *    └─ Reacts to hashrate changes in 3-5 blocks (vs Monero's days)
 *    └─ Formula: next_diff = (sum_diff × T × (N+1)) / (2 × sum_weighted_times)
 *    
 * 2. DIFFICULTY_RESET_HEIGHT = 4726
 *    └─ One-time reset when large miner departed (height 4724)
 *    └─ Difficulty dropped from 9.15M to ~68K in 60 blocks
 *    └─ Prevented LWMA from looking back at inflated pre-departure blocks
 *    
 * 3. EDA (Emergency Difficulty Adjustment)
 *    └─ Trigger: Block takes > 720s (6 × 120s target)
 *    └─ Action: adjusted_diff = last_diff × target / actual_time
 *    └─ Result: Uses min(LWMA, adjusted) to cap difficulty
 *    └─ Recovery: 99% hashrate loss recovered in 1-2 blocks
 * 
 * 4. Clamping Protection
 *    └─ Each solve time clamped to [-720s, +720s]
 *    └─ Prevents timestamp attacks, slow block exaggeration
 */
class AIHashrateRecoveryMonitor {
public:
    /**
     * @brief Network difficulty state snapshot
     */
    struct DifficultyState {
        uint64_t height;                           // Current block height
        uint64_t current_difficulty;               // Current target difficulty
        uint64_t average_block_time;               // Recent avg solve time (seconds)
        uint64_t expected_block_time;              // Target block time (120s)
        double   difficulty_ratio;                 // current / expected
        std::string state_name;                    // "STABLE", "ADJUSTING", "RECOVERING"
        time_t   measured_timestamp;               // When this snapshot was taken
    };

    /**
     * @brief Recovery event from hashrate drop
     */
    struct RecoveryEvent {
        uint64_t drop_height;                      // Height when drop first detected
        uint64_t start_difficulty;                 // Difficulty at drop
        uint64_t end_height;                       // Height when recovered
        uint64_t end_difficulty;                   // Final recovered difficulty
        uint64_t hashrate_loss_percent;            // Estimated % hashrate lost
        uint64_t recovery_blocks;                  // Blocks to recover
        double   recovery_speed;                   // difficulty_adjustment_per_block
        std::string recovery_type;                 // "LWMA", "EDA", "COMBINED"
        time_t   event_timestamp;
    };

    /**
     * @brief LWMA window state (last 60 blocks)
     */
    struct LWMAWindowState {
        uint64_t window_start_height;              // Oldest block in window
        uint64_t window_end_height;                // Newest block in window
        uint64_t total_blocks_in_window;
        double   average_solve_time;               // Mean time between blocks
        double   weighted_solve_time;              // LWMA weighted average
        double   difficulty_contribution;         // Weighted difficulty sum
        std::string window_status;                 // "FILLING", "ACTIVE", "FULL"
    };

    /**
     * @brief EDA activation history
     */
    struct EDAEvent {
        uint64_t height;                           // Block height where triggered
        uint64_t solve_time;                       // Block solve time in seconds
        uint64_t threshold_seconds;                // EDA threshold (720s)
        uint64_t lwma_difficulty;                  // LWMA calculated difficulty
        uint64_t eda_adjusted_difficulty;          // EDA calculated difficulty
        uint64_t final_difficulty;                 // What was actually used
        std::string reason;                        // Why EDA was triggered
        time_t   event_timestamp;
    };

    /**
     * @brief IA's hashrate recovery knowledge base
     */
    struct HashrateKnowledge {
        // System parameters (from cryptonote_config.h)
        uint64_t difficulty_reset_height;          // = 4726
        uint64_t eda_threshold_multiplier;         // = 6 (× 120s = 720s)
        uint64_t target_block_time;                // = 120 seconds
        uint64_t lwma_window_size;                 // = 60 blocks
        
        // Network state
        DifficultyState current_state;             // Latest difficulty snapshot
        std::deque<DifficultyState> history;       // Last 100 states
        
        // LWMA window
        LWMAWindowState lwma_window;               // Current LWMA window state
        
        // Recovery tracking
        std::vector<RecoveryEvent> recovery_events;// All recovery events detected
        RecoveryEvent last_recovery;               // Most recent recovery
        
        // EDA tracking
        std::vector<EDAEvent> eda_events;          // All EDA activations
        uint64_t eda_activation_count;             // Total EDA triggers
        
        // Statistics
        uint64_t total_blocks_monitored;
        double   average_network_hashrate;         // Estimated from difficulty
        uint64_t estimated_miners_count;           // Based on variance
    };

    /**
     * @brief Initialize hashrate recovery monitoring
     */
    static void ia_initialize_hashrate_learning();

    /**
     * @brief IA learns current difficulty state
     */
    static void ia_learns_difficulty_state(
        uint64_t height,
        uint64_t current_difficulty,
        uint64_t block_solve_time,
        bool eda_activated = false
    );

    /**
     * @brief IA detects if recovery is happening
     */
    static bool ia_detect_recovery_in_progress(
        const HashrateKnowledge& knowledge,
        uint64_t& estimated_recovery_blocks
    );

    /**
     * @brief IA learns EDA activation
     */
    static void ia_learn_eda_event(
        uint64_t height,
        uint64_t actual_solve_time,
        uint64_t base_difficulty
    );

    /**
     * @brief Get the global hashrate knowledge base
     */
    static const HashrateKnowledge& ia_get_hashrate_knowledge();

    /**
     * @brief Reset learning data (for testing/reset)
     */
    static void ia_reset_hashrate_learning();

    /**
     * @brief IA analyzes LWMA window health
     */
    static std::string ia_analyze_lwma_window(const HashrateKnowledge& knowledge);

    /**
     * @brief IA predicts next difficulty
     */
    static uint64_t ia_predict_next_difficulty(
        const HashrateKnowledge& knowledge,
        bool considering_eda = true
    );

    /**
     * @brief IA estimates network hashrate from difficulty
     */
    static double ia_estimate_network_hashrate(
        uint64_t difficulty,
        uint64_t target_block_time
    );

    /**
     * @brief IA detects sudden hashrate drops
     */
    static bool ia_detect_hashrate_anomaly(
        const HashrateKnowledge& knowledge,
        double& hashrate_change_percent
    );

    /**
     * @brief IA provides recovery recommendations
     */
    static std::string ia_recommend_hashrate_recovery(const HashrateKnowledge& knowledge);

    /**
     * @brief IA logs system status report
     */
    static void ia_log_hashrate_status(const HashrateKnowledge& knowledge);

    /**
     * @brief IA calculates optimal difficulty for stability
     */
    static uint64_t ia_calculate_optimal_difficulty(
        const HashrateKnowledge& knowledge,
        uint64_t observed_hashrate
    );

    // =====================================================================
    // NINA AS NETWORK CONNECTOR - NEW FUNCTIONS
    // =====================================================================
    
    /**
     * @brief NINA welcomes a new node to the ninacatcoin network
     * @param node_height Height of the new node's blockchain
     * @param node_difficulty Current difficulty at that height
     * @return Integration status message
     * 
     * NINA's role as Connector:
     *   - Greets new nodes joining the network
     *   - Helps them synchronize properly
     *   - Connects them to reliable peers
     *   - Integrates them into ONE unified network
     */
    static std::string nina_welcome_new_node(uint64_t node_height, uint64_t node_difficulty);

    /**
     * @brief NINA ensures a node stays connected to the network
     * @param node_height Current height
     * @param node_peer_count Number of connected peers
     * @return Connection health status
     * 
     * NINA's role as Connector:
     *   - Monitors node connectivity
     *   - Alerts if node is falling behind
     *   - Reconnects isolated nodes
     *   - Maintains network cohesion
     */
    static std::string nina_maintain_node_connection(uint64_t node_height, int node_peer_count);

    /**
     * @brief NINA validates and protects a miner's block
     * @param miner_height Height of the mined block
     * @param block_difficulty Difficulty of the block
     * @param block_hash Hash of the block
     * @return Validation and protection status
     * 
     * NINA's role as Mining Guardian:
     *   - Verifies block is legitimate
     *   - Protects against orphaning
     *   - Ensures global propagation
     *   - Guarantees miner's reward is secure
     */
    static std::string nina_protect_mining_work(
        uint64_t miner_height,
        uint64_t block_difficulty,
        const std::string& block_hash
    );

    /**
     * @brief NINA reports the health of the entire network
     * @return Comprehensive network health report
     * 
     * NINA's role as Network Guardian:
     *   - Shows how many nodes are connected
     *   - Shows total network hashrate
     *   - Shows consensus strength
     *   - Shows network efficiency
     *   - Alerts about issues
     */
    static std::string nina_report_network_health();

    /**
     * @brief NINA's core purpose - maintain ONE unified network
     * @return Message about network unity status
     * 
     * NINA exists to ensure:
     *   - New nodes are welcomed and connected
     *   - Old nodes stay connected and updated
     *   - All nodes work as ONE entity
     *   - Miners' work is protected
     *   - Network is efficient and resilient
     */
    static std::string nina_ensure_network_unity();

    // ========================================================================
    // NINA AS CHECKPOINT GUARDIAN - Protect against 51% attacks
    // ========================================================================

    /**
     * @brief NINA validates checkpoint integrity and authenticity
     * @param checkpoint_height Height of checkpoint
     * @param checkpoint_hash Hash of checkpoint block
     * @param num_verifying_peers How many peers confirm this checkpoint
     * @return Validation result with security status
     * 
     * NINA protects against:
     *   - Modified checkpoints
     *   - Man-in-the-middle attacks
     *   - Compromised servers
     *   - Invalid file format
     *   - Forged hashes
     */
    static std::string nina_validate_checkpoint_integrity(
        uint64_t checkpoint_height,
        const std::string& checkpoint_hash,
        int num_verifying_peers
    );

    /**
     * @brief NINA verifies checkpoint source reliability
     * @param source_peer_id ID of peer providing checkpoint
     * @param checkpoint_height Height of checkpoint
     * @param download_protocol Protocol used (SFTP, HTTP, DNS)
     * @return Source verification status
     * 
     * NINA detects:
     *   - Malicious peers
     *   - Compromised servers
     *   - Single-source attacks
     *   - DNS poisoning
     *   - SFTP interception
     */
    static std::string nina_verify_checkpoint_source(
        const std::string& source_peer_id,
        uint64_t checkpoint_height,
        const std::string& download_protocol
    );

    /**
     * @brief NINA detects fork attempts using checkpoint analysis
     * @param chain_a_height Height of checkpoint from chain A
     * @param chain_a_hash Hash from chain A
     * @param chain_b_height Height of checkpoint from chain B
     * @param chain_b_hash Hash from chain B
     * @return Fork detection result and alert status
     * 
     * NINA identifies:
     *   - Concurrent valid chains
     *   - 51% attack fork attempts
     *   - Consensus break
     *   - Diverging networks
     */
    static std::string nina_detect_checkpoint_fork(
        uint64_t chain_a_height,
        const std::string& chain_a_hash,
        uint64_t chain_b_height,
        const std::string& chain_b_hash
    );

    /**
     * @brief NINA actively protects checkpoint download process
     * @param download_url URL to download from
     * @param expected_size Expected file size in bytes
     * @param timeout_seconds Maximum download time
     * @return Real-time protection status during download
     * 
     * NINA monitors:
     *   - Man-in-the-middle interference
     *   - File corruption during transfer
     *   - Suspicious timing
     *   - Size mismatches
     *   - Connection anomalies
     */
    static std::string nina_protect_checkpoint_download(
        const std::string& download_url,
        uint64_t expected_size,
        int timeout_seconds
    );

    /**
     * @brief NINA enforces consensus on checkpoint validity
     * @param checkpoint_height Height being verified
     * @param peer_confirmations Map of peer_id -> hash agreement
     * @param consensus_threshold Percentage threshold (e.g., 95)
     * @return Consensus validation result
     * 
     * NINA requires:
     *   - 95%+ of peers agree on hash
     *   - Majority verification before acceptance
     *   - Immediate rejection if consensus lost
     *   - Network unity maintained
     */
    static std::string nina_enforce_checkpoint_consensus(
        uint64_t checkpoint_height,
        const std::map<std::string, std::string>& peer_confirmations,
        int consensus_threshold
    );

    /**
     * @brief NINA verifies checkpoint comes from official seed node
     * @param checkpoint_source_ip IP address claiming to provide checkpoint
     * @param checkpoint_type Type of checkpoint (JSON or DAT)
     * @return Verification status - ACCEPT or REJECT
     * 
     * NINA's Official Seed Nodes:
     *   - Seed1: 87.106.7.156 (provides checkpoints.json every hour)
     *   - Seed2: 217.154.196.9 (provides checkpoints.dat every hour)
     * 
     * ONLY these two seeds are authorized sources.
     * Any checkpoint from other IP = AUTOMATIC REJECTION
     * Any attempt to spoof these IPs = CRITICAL ALERT + BAN
     */
    static std::string nina_verify_seed_node_source(
        const std::string& checkpoint_source_ip,
        const std::string& checkpoint_type
    );

    /**
     * @brief NINA validates checkpoint from official seed node
     * @param seed_node_name Name of seed (Seed1 or Seed2)
     * @param checkpoint_height Height of checkpoint being validated
     * @param checkpoint_content Content/hash of checkpoint
     * @return Comprehensive validation report
     * 
     * NINA performs complete security check:
     *   - Verifies source IP matches registered seed
     *   - Validates checkpoint format
     *   - Confirms cryptographic signature
     *   - Ensures no tampering occurred
     *   - Broadcasts to network for consensus
     */
    static std::string nina_validate_seed_node_checkpoint(
        const std::string& seed_node_name,
        uint64_t checkpoint_height,
        const std::string& checkpoint_content
    );

    /**
     * @brief NINA monitors seed node health and synchronization
     * @return Status report of both seed nodes
     * 
     * NINA continuously checks:
     *   - Is Seed1 online and responding?
     *   - Is Seed2 online and responding?
     *   - Are they generating checkpoints on schedule (every hour)?
     *   - Are they in agreement with each other?
     *   - Is there any consensus break between them?
     * 
     * If either seed goes offline or stops generating:
     *   - IMMEDIATE ALERT to all nodes
     *   - Network enters "CHECKPOINT EMERGENCY MODE"
     *   - Use cached recent checkpoints only
     */
    static std::string nina_monitor_seed_nodes_health();

    /**
     * @brief NINA validates checkpoint epoch_id and timestamp freshness
     * @param current_epoch_id New epoch_id from checkpoint
     * @param previous_epoch_id Last known epoch_id
     * @param generated_at_ts Unix timestamp of generation
     * @param current_time Current Unix timestamp
     * @return Freshness validation result
     * 
     * CHECKPOINT UPDATE VALIDATION:
     *   - epoch_id MUST be HIGHER than previous (monotonically increasing)
     *   - If epoch_id SAME → Seed didn't generate new checkpoint (stale)
     *   - If epoch_id LOWER → ATTACK or CORRUPTION (reject immediately)
     *   - generated_at_ts MUST be within last hour (3600 seconds)
     *   - If timestamp > 1 hour old → Seed offline (emergency alert)
     * 
     * NINA learns seed behavior patterns:
     *   - Seed generates every ~3600 seconds (1 hour)
     *   - epoch_id increments with each generation
     *   - Can predict when next checkpoint should arrive
     */
    static std::string nina_validate_checkpoint_epoch(
        uint64_t current_epoch_id,
        uint64_t previous_epoch_id,
        uint64_t generated_at_ts,
        uint64_t current_time
    );

    /**
     * @brief NINA detects if seed node has stopped generating checkpoints
     * @param seed_node_name Name of seed (Seed1 or Seed2)
     * @param last_epoch_id Last received epoch_id
     * @param time_since_last_update Seconds since last checkpoint update
     * @return Status: HEALTHY, LAGGING, STALE, or OFFLINE
     * 
     * TIMEOUT DETECTION:
     *   - Seed should generate every ~60 minutes
     *   - If no new epoch_id after 70 minutes → WARNING
     *   - If no new epoch_id after 120 minutes → CRITICAL (OFFLINE)
     *   - If no new epoch_id after 180 minutes → EMERGENCY MODE activated
     */
    static std::string nina_monitor_checkpoint_generation_frequency(
        const std::string& seed_node_name,
        uint64_t last_epoch_id,
        uint64_t time_since_last_update
    );

    /**
     * @brief NINA compares epoch_id between Seed1 and Seed2 for consensus
     * @param seed1_epoch_id epoch_id from Seed1 (JSON)
     * @param seed2_epoch_id epoch_id from Seed2 (DAT)
     * @param seed1_timestamp generated_at_ts from Seed1
     * @param seed2_timestamp generated_at_ts from Seed2
     * @return Consensus validation: SYNCHRONIZED, LAGGING, DIVERGED, or FORK
     * 
     * SEED CONSENSUS CHECK:
     *   - Both seeds MUST have same or nearly same epoch_id
     *   - If Seed1 epoch > Seed2 epoch by >2 → Seed2 lagging (WARNING)
     *   - If Seed1 epoch < Seed2 epoch → Impossible (Seed1 generates first)
     *   - If |epoch1 - epoch2| > 5 → Severe desynchronization (CRITICAL)
     *   - If both have older timestamps → Both offline (EMERGENCY)
     */
    static std::string nina_verify_seed_consensus_epoch(
        uint64_t seed1_epoch_id,
        uint64_t seed2_epoch_id,
        uint64_t seed1_timestamp,
        uint64_t seed2_timestamp
    );

    /**
     * @brief NINA predicts when next checkpoint should arrive based on pattern
     * @param last_epoch_id Last received epoch_id
     * @param last_generated_at_ts When last checkpoint was generated
     * @param current_time Current Unix timestamp
     * @return Next expected generation time and alert if overdue
     * 
     * PREDICTIVE MONITORING:
     *   - NINA learns seed generates ~every 3600 seconds
     *   - Can predict: next_generation_time = last_generated_at_ts + 3600
     *   - If current_time > next_generation_time + 300 → Warning
     *   - If current_time > next_generation_time + 900 → Critical
     */
    static std::string nina_predict_next_checkpoint_arrival(
        uint64_t last_epoch_id,
        uint64_t last_generated_at_ts,
        uint64_t current_time
    );

    /**
     * @brief NINA alerts about checkpoint compromise
     */
    static std::string nina_alert_checkpoint_compromise(
        const std::string& threat_type,
        uint64_t affected_height,
        const std::string& alert_severity
    );

    /**
     * @brief NINA alerts about seed node issues
     */
    static std::string nina_alert_seed_node_issue(
        const std::string& seed_node_ip,
        const std::string& issue_type
    );
};

} // namespace ninacatcoin_ai
