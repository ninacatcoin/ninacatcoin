// Copyright (c) 2026, The ninacatcoin Project
//
// IA CHECKPOINT MONITORING - INTEGRATION EXAMPLES
// ═════════════════════════════════════════════════════════════════════════════
//
// This file shows practical examples of how to integrate checkpoint monitoring
// into the existing ninacatcoin daemon code.
//
// NOTE: These are examples. Actual implementation should integrate these calls
// into the appropriate existing code locations.

#pragma once

#include "ai_checkpoint_monitor.hpp"
#include <iostream>

namespace ninacatcoin_ai {

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 1: Integrating with daemon.cpp startup
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/daemon/daemon.cpp in Daemon class constructor

ORIGINAL CODE:
    Daemon::Daemon() : m_running(true) {
        MINFO("Starting daemon...");
        initialize_blockchain();
        initialize_p2p();
    }

MODIFIED CODE (with IA checkpoint monitoring):
*/

void example_daemon_startup_with_checkpoints() {
    MINFO("═════════════════════════════════════════════════════════");
    MINFO("[DAEMON] Starting ninacatcoin daemon with IA monitoring");
    MINFO("═════════════════════════════════════════════════════════");
    
    // STEP 1: Initialize IA Module (MUST be first)
    MINFO("[DAEMON] Step 1: Initializing IA Module...");
    // AIModule::getInstance().initialize();
    
    // STEP 2: Initialize IA checkpoint monitoring
    MINFO("[DAEMON] Step 2: Initializing IA Checkpoint Monitoring...");
    ia_checkpoint_monitor_initialize();
    MINFO("[DAEMON] ✓ Checkpoint monitor ready");
    
    // STEP 3: Set network type
    MINFO("[DAEMON] Step 3: Setting network type...");
    ia_set_checkpoint_network("mainnet");  // or testnet, stagenet
    MINFO("[DAEMON] ✓ Network type: mainnet");
    
    // STEP 4: Initialize blockchain (loads checkpoints)
    MINFO("[DAEMON] Step 4: Initializing blockchain...");
    // blockchain::init();  // This would load checkpoints
    // When checkpoints are loaded, call:
    // ia_register_checkpoint(height, hash, difficulty, "compiled");
    // ia_register_checkpoint(height, hash, difficulty, "json");
    
    // STEP 5: IA analyzes checkpoint distribution
    MINFO("[DAEMON] Step 5: IA analyzing checkpoint distribution...");
    ia_print_checkpoint_status();
    MINFO("");
    MINFO(ia_get_verification_strategy());
    MINFO("");
    MINFO(ia_get_checkpoint_optimization_recommendations());
    
    // STEP 6: Initialize P2P networking
    MINFO("[DAEMON] Step 6: Initializing P2P networking...");
    // p2p_network::init();
    
    MINFO("═════════════════════════════════════════════════════════");
    MINFO("[DAEMON] Daemon initialized. IA watching the network.");
    MINFO("═════════════════════════════════════════════════════════");
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 2: Registering checkpoints when they're loaded
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/checkpoints/checkpoints.cpp in the add_checkpoint() function

ORIGINAL CODE:
    void checkpoints::add_checkpoint(uint64_t height, const std::string& hash) {
        m_checkpoints[height] = hash;
    }

MODIFIED CODE (with IA monitoring):
*/

void example_registering_checkpoint(
    uint64_t height,
    const std::string& hash_hex,
    const std::string& difficulty,
    const std::string& source
) {
    // Add to normal checkpoint storage
    // m_checkpoints[height] = hash_hex;
    
    // Register with IA for monitoring
    ia_register_checkpoint(height, hash_hex, difficulty, source);
    
    MDEBUG("[CHECKPOINTS] Added checkpoint at height " << height 
           << " from " << source);
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 3: Validating a block against checkpoints (in blockchain validation)
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/blockchain/blockchain.cpp in validate_block() function

ORIGINAL CODE:
    bool Blockchain::validate_block(const Block& block) {
        if (!validate_pow(block)) return false;
        if (!validate_timestamp(block)) return false;
        return true;
    }

MODIFIED CODE (with IA checkpoint validation):
*/

bool example_validate_block_against_checkpoints(
    uint64_t block_height,
    const std::string& block_hash
) {
    MDEBUG("[BLOCKCHAIN] Validating block at height " << block_height);
    
    // STEP 1: IA verifies against known checkpoints
    if (!ia_verify_block_against_checkpoints(block_height, block_hash)) {
        MERROR("[BLOCKCHAIN] ✗ BLOCK REJECTED: Hash doesn't match checkpoint");
        MERROR("[BLOCKCHAIN]   Height: " << block_height);
        MERROR("[BLOCKCHAIN]   Hash: " << block_hash);
        return false;  // REJECT BLOCK
    }
    
    MDEBUG("[BLOCKCHAIN] ✓ Block checkpoint verification passed");
    
    // STEP 2: Continue with normal validation
    // if (!validate_pow(block)) return false;
    // if (!validate_timestamp(block)) return false;
    
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 4: Detecting forks when receiving peer data (in P2P code)
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/p2p/p2p_peer_validation.cpp when receiving peer info

ORIGINAL CODE:
    void P2P::validate_peer(const Peer& peer) {
        if (peer.version != our_version) peer.set_untrusted();
    }

MODIFIED CODE (with IA fork detection):
*/

bool example_detect_fork_from_peer(
    const AICheckpointMonitor::CheckpointKnowledge& peer_checkpoints
) {
    MINFO("[P2P] Verifying peer checkpoint compatibility...");
    
    // Have IA compare peer's checkpoints with ours
    uint64_t conflict_height = 0;
    
    if (ia_detect_checkpoint_fork(peer_checkpoints, conflict_height)) {
        MERROR("[P2P] ✗ FORK DETECTED with this peer!");
        MERROR("[P2P]   Conflict height: " << conflict_height);
        MERROR("[P2P]   Peer is on a different blockchain");
        MERROR("[P2P]   Marking peer as UNTRUSTED");
        
        // Mark peer as untrusted
        // peer.set_untrusted();
        // peer.quarantine(86400);  // 24 hours
        
        return true;  // Fork detected
    }
    
    MINFO("[P2P] ✓ Peer checkpoints compatible");
    return false;  // No fork
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 5: IA's main monitoring loop - checkpoint health check
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/ai/ai_module.cpp in AIModule::monitor_loop()

This is the IA's continuous monitoring loop (runs every 60 seconds)
*/

void example_checkpoint_monitoring_loop() {
    static uint32_t loop_count = 0;
    loop_count++;
    
    // Get current checkpoint knowledge
    auto cp_knowledge = ia_get_checkpoint_knowledge();
    
    if (cp_knowledge.total_checkpoints == 0) {
        MWARNING("[IA MONITOR] No checkpoints loaded yet");
        return;
    }
    
    // DIAGNOSTIC: Every minute, log checkpoint status
    if (loop_count % 1 == 0) {
        MDEBUG("[IA MONITOR] Checkpoint health check #" << loop_count);
        MDEBUG("[IA MONITOR]   Total checkpoints: " << cp_knowledge.total_checkpoints);
        MDEBUG("[IA MONITOR]   Height range: " 
               << cp_knowledge.earliest_checkpoint_height << " - "
               << cp_knowledge.latest_checkpoint_height);
    }
    
    // DIAGNOSTIC: Every 10 minutes, detailed analysis
    if (loop_count % 10 == 0) {
        MINFO("[IA MONITOR] Detailed checkpoint analysis (every 10 min):");
        ia_print_checkpoint_analysis();
    }
    
    // ALERT: Check for abnormalies
    
    // Alert 1: Checkpoints haven't updated in too long
    static time_t last_checkpoint_update = 0;
    time_t now = time(nullptr);
    
    if (last_checkpoint_update > 0 && (now - last_checkpoint_update) > 86400) {
        MWARNING("[IA MONITOR] ⚠️  No new checkpoints in 24 hours!");
        MWARNING("[IA MONITOR]   Last checkpoint update: " << last_checkpoint_update);
        MWARNING("[IA MONITOR]   This might indicate network issues");
    }
    
    last_checkpoint_update = now;
    
    // Alert 2: Extremely few checkpoints
    if (cp_knowledge.total_checkpoints < 5) {
        MWARNING("[IA MONITOR] ⚠️  Very few checkpoints: " << cp_knowledge.total_checkpoints);
        MWARNING("[IA MONITOR]   Network may be young or incomplete");
    }
    
    // Alert 3: Checkpoint height far behind current
    uint64_t current_height = 0;  // Would get from blockchain
    if (current_height > cp_knowledge.latest_checkpoint_height + 1000000) {
        MWARNING("[IA MONITOR] ⚠️  Latest checkpoint far behind current height");
        MWARNING("[IA MONITOR]   Latest checkpoint: " << cp_knowledge.latest_checkpoint_height);
        MWARNING("[IA MONITOR]   Current height: " << current_height);
        MWARNING("[IA MONITOR]   Consider updating checkpoints");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 6: Debug commands for checkpoint information
// ═════════════════════════════════════════════════════════════════════════════

/*
These could be added as RPC commands or command-line options
for daemon operators to inspect checkpoint status
*/

void example_debug_commands() {
    // Command: "ia checkpoint status"
    {
        MINFO("Executing: ia checkpoint status");
        ia_print_checkpoint_status();
    }
    
    // Command: "ia checkpoint analysis"
    {
        MINFO("Executing: ia checkpoint analysis");
        ia_print_checkpoint_analysis();
    }
    
    // Command: "ia checkpoint details"
    {
        MINFO("Executing: ia checkpoint details");
        ia_print_checkpoint_details();
    }
    
    // Command: "ia checkpoint optimize"
    {
        MINFO("Executing: ia checkpoint optimize");
        MINFO(ia_get_checkpoint_optimization_recommendations());
    }
    
    // Command: "ia checkpoint verify"
    {
        MINFO("Executing: ia checkpoint verify");
        MINFO(ia_get_verification_strategy());
    }
    
    // Command: "ia checkpoint count"
    {
        MINFO("Executing: ia checkpoint count");
        MINFO("Total checkpoints: " << ia_get_checkpoint_count());
        MINFO("Latest height: " << ia_get_latest_checkpoint_height());
        MINFO("Earliest height: " << ia_get_earliest_checkpoint_height());
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 7: Loading checkpoints from JSON (with IA integration)
// ═════════════════════════════════════════════════════════════════════════════

/*
LOCATION: src/checkpoints/checkpoints.cpp in load_from_json()

This shows how to register each JSON checkpoint with IA as they're loaded.
*/

void example_load_checkpoints_from_json(const std::string& json_path) {
    MINFO("[CHECKPOINTS] Loading checkpoints from JSON: " << json_path);
    
    // Parse JSON file
    // std::ifstream file(json_path);
    // nlohmann::json j;
    // file >> j;
    
    // Register each checkpoint with both normal storage and IA
    // for (const auto& checkpoint : j["checkpoints"]) {
    //     uint64_t height = checkpoint["height"];
    //     std::string hash = checkpoint["hash"];
    //     std::string difficulty = checkpoint["difficulty"];
    //     
    //     // Add to normal checkpoint map
    //     m_checkpoints[height] = hash;
    //     
    //     // Register with IA
    //     ia_register_checkpoint(height, hash, difficulty, "json");
    // }
    
    MINFO("[CHECKPOINTS] JSON checkpoints loaded and registered with IA");
}

// ═════════════════════════════════════════════════════════════════════════════
// EXAMPLE 8: Full daemon startup sequence with checkpoint monitoring
// ═════════════════════════════════════════════════════════════════════════════

void example_complete_daemon_startup_sequence() {
    MINFO("");
    MINFO("╔════════════════════════════════════════════════════════════╗");
    MINFO("║   NINACATCOIN DAEMON STARTUP WITH IA CHECKPOINT MONITOR   ║");
    MINFO("╚════════════════════════════════════════════════════════════╝");
    MINFO("");
    
    // Phase 1: IA Initialization
    MINFO("[STARTUP] Phase 1: IA Initialization");
    MINFO("  └─ Initializing IA Module...");
    // AIModule::getInstance().initialize();
    MINFO("    ✓ IA Module ready");
    
    // Phase 2: Checkpoint Monitoring Init
    MINFO("");
    MINFO("[STARTUP] Phase 2: Checkpoint Monitoring Init");
    MINFO("  └─ Initializing checkpoint monitor...");
    ia_checkpoint_monitor_initialize();
    MINFO("    ✓ Checkpoint monitor ready");
    
    // Phase 3: Load Checkpoints
    MINFO("");
    MINFO("[STARTUP] Phase 3: Loading Checkpoints");
    MINFO("  ├─ Loading compiled checkpoints...");
    ia_set_checkpoint_network("mainnet");
    // Load compiled checkpoints and register with IA
    // ia_register_checkpoint(1000000, "hash1", "difficulty1", "compiled");
    // ia_register_checkpoint(2000000, "hash2", "difficulty2", "compiled");
    MINFO("    ✓ Compiled checkpoints loaded");
    
    MINFO("  ├─ Loading JSON checkpoints...");
    // Load JSON checkpoints
    MINFO("    ✓ JSON checkpoints loaded");
    
    MINFO("  └─ Attempting DNS checkpoints...");
    // Try to load DNS checkpoints
    MINFO("    ✓ DNS checkpoints loaded");
    
    // Phase 4: IA Analysis
    MINFO("");
    MINFO("[STARTUP] Phase 4: IA Checkpoint Analysis");
    ia_print_checkpoint_status();
    
    // Phase 5: Network Initialization
    MINFO("");
    MINFO("[STARTUP] Phase 5: Network Initialization");
    MINFO("  ├─ Initializing P2P network...");
    // p2p::init();
    MINFO("    ✓ P2P network ready");
    
    MINFO("  └─ Initializing blockchain sync...");
    // blockchain::init_sync();
    MINFO("    ✓ Blockchain sync ready");
    
    // Phase 6: Ready
    MINFO("");
    MINFO("╔════════════════════════════════════════════════════════════╗");
    MINFO("║      DAEMON STARTED SUCCESSFULLY                           ║");
    MINFO("║      IA IS MONITORING THE NETWORK                         ║");
    MINFO("║      Blockchain synchronization starting...              ║");
    MINFO("╚════════════════════════════════════════════════════════════╝");
    MINFO("");
}

} // namespace ninacatcoin_ai
