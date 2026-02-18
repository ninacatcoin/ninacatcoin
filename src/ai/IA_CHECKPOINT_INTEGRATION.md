// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.

#pragma once

/*
 * IA CHECKPOINT INTEGRATION GUIDE
 * ════════════════════════════════════════════════════════════════════════════
 * 
 * This document explains how to integrate the checkpoint monitoring system
 * into the IA module and daemon.
 * 
 * OVERVIEW:
 * ═════════
 * The IA learns how checkpoints work and monitors them to ensure network integrity.
 * Checkpoints are critical for:
 * - Fast blockchain synchronization
 * - Preventing long-range attacks
 * - Detecting network forks
 * - Providing trusted reference points
 * 
 * INTEGRATION POINTS:
 * ═══════════════════
 */

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 1. IN ai_module.hpp - Add header includes                                  ║
// ╚════════════════════════════════════════════════════════════════════════════╝

// ADD THIS TO ai_module.hpp:

// #include "ai_checkpoint_monitor.hpp"

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 2. IN AIMODULE::INITIALIZE() - Initialize checkpoint monitoring           ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
In AIModule::initialize():

    MINFO("[IA MODULE] Initializing IA components...");
    
    // Initialize checkpoint monitoring FIRST
    ia_checkpoint_monitor_initialize();
    MINFO("[IA] Checkpoint monitoring system initialized");
    
    // ... rest of initialization ...
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 3. WHEN LOADING CHECKPOINTS - Register with IA                            ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
In checkpoints.cpp add_checkpoint() function:

    // Register checkpoint with IA monitoring
    ia_set_checkpoint_network("mainnet");  // or testnet, stagenet
    ia_register_checkpoint(
        height,
        hash_hex_string,
        difficulty_string,
        "compiled"  // or "json", "dns"
    );
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 4. IN BLOCKCHAIN VALIDATION - Verify blocks against checkpoints           ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
In blockchain validation (e.g., validate_block()):

    // Have IA verify block against known checkpoints
    if (!ia_verify_block_against_checkpoints(block.height, block.hash)) {
        MERROR("Block rejected: Checkpoint mismatch");
        return false;
    }
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 5. IN DAEMON STARTUP - Print checkpoint analysis                          ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
In daemon.cpp after initialization:

    // Print IA's checkpoint analysis
    ia_print_checkpoint_status();
    MINFO(ia_get_checkpoint_optimization_recommendations());
    MINFO(ia_get_verification_strategy());
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ 6. IN PEER MONITORING - Compare checkpoints with peers                    ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
In ia_peer_monitoring.h when receiving peer info:

    // Check if peer has conflicting checkpoints
    uint64_t conflict_height = 0;
    if (ia_detect_checkpoint_fork(peer_checkpoints, conflict_height)) {
        MERROR("Peer has conflicting checkpoint at height " << conflict_height);
        // Quarantine peer or mark as untrustworthy
    }
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ FUNCTION REFERENCE                                                        ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
Core Functions (defined in ai_checkpoint_monitor.cpp):

1. ia_checkpoint_monitor_initialize()
   Purpose: Initialize the checkpoint monitoring system
   When: Called once during daemon startup
   Example:
       ia_checkpoint_monitor_initialize();

2. ia_set_checkpoint_network(const std::string& network_type)
   Purpose: Tell IA which network we're on
   When: When daemon determines network (mainnet/testnet/stagenet)
   Example:
       ia_set_checkpoint_network("mainnet");

3. ia_register_checkpoint(uint64_t height, const std::string& hash, 
                          const std::string& difficulty, const std::string& source)
   Purpose: Register a checkpoint with the IA monitor
   When: Every time a checkpoint is loaded
   Example:
       ia_register_checkpoint(100000, "abc123...", "999999", "json");

4. ia_verify_block_against_checkpoints(uint64_t block_height, const std::string& block_hash)
   Purpose: Check if a block matches any known checkpoint
   Returns: true if valid, false if mismatch
   When: During block validation
   Example:
       if (!ia_verify_block_against_checkpoints(height, hash)) {
           // Block failed checkpoint verification
       }

5. ia_get_checkpoint_knowledge()
   Purpose: Get current checkpoint database
   Returns: CheckpointKnowledge struct with all data
   When: For analysis or comparisons
   Example:
       auto knowledge = ia_get_checkpoint_knowledge();
       MINFO("Total checkpoints: " << knowledge.total_checkpoints);

6. ia_detect_checkpoint_fork(const CheckpointKnowledge& peer_knowledge, 
                             uint64_t& conflict_height)
   Purpose: Detect if a peer has conflicting checkpoints (fork indicator)
   Returns: true if fork detected
   When: When comparing with peer checkpoint data
   Example:
       uint64_t conflict = 0;
       if (ia_detect_checkpoint_fork(peer_cp, conflict)) {
           MERROR("Fork at height " << conflict);
       }

7. ia_print_checkpoint_status()
   Purpose: Print detailed status report to logs
   When: On startup or via debug command
   Example:
       ia_print_checkpoint_status();

8. ia_print_checkpoint_analysis()
   Purpose: Print checkpoint distribution analysis
   When: On startup or via debug command
   Example:
       ia_print_checkpoint_analysis();

9. ia_get_checkpoint_optimization_recommendations()
   Purpose: Get suggestions for optimizing checkpoint loading
   Returns: std::string with recommendations
   When: On startup or for optimization analysis
   Example:
       MINFO(ia_get_checkpoint_optimization_recommendations());

10. ia_get_verification_strategy()
    Purpose: Get recommended block verification strategy
    Returns: std::string explaining verification approach
    When: On startup
    Example:
        MINFO(ia_get_verification_strategy());

11. ia_get_checkpoint_count()
    Purpose: Get number of monitored checkpoints
    Returns: uint64_t count
    When: For status queries
    Example:
        MINFO("IA monitoring " << ia_get_checkpoint_count() << " checkpoints");

12. ia_get_latest_checkpoint_height()
    Purpose: Get the height of the newest checkpoint
    Returns: uint64_t height
    When: For sync status
    Example:
        uint64_t latest = ia_get_latest_checkpoint_height();

13. ia_get_earliest_checkpoint_height()
    Purpose: Get the height of the oldest checkpoint
    Returns: uint64_t height
    When: For analyzing checkpoint range
    Example:
        uint64_t earliest = ia_get_earliest_checkpoint_height();

14. ia_print_checkpoint_details()
    Purpose: Print complete details of all known checkpoints
    When: For debugging or detailed analysis
    Example:
        ia_print_checkpoint_details();
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ CHECKPOINT CONCEPT (What IA learns)                                       ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
A checkpoint in ninacatcoin is:

    Height: Integer block number
    ├─ Example: 1000000
    
    Hash: SHA-256 hash of the block
    ├─ Example: "abc123def456..."
    ├─ Used to: Verify if a block is correct
    └─ Format: Hexadecimal string (64 chars)
    
    Difficulty: Cumulative difficulty at that height
    ├─ Example: "123456789012345"
    ├─ Used to: Understand network power over time
    └─ Format: Large integer as string
    
    Source: Where the checkpoint came from
    ├─ "compiled": Hardcoded in ninacatcoin binary
    ├─ "json": Loaded from checkpoints.json file
    └─ "dns": Fetched from DNS checkpoint servers

CHECKPOINT PURPOSES:
════════════════════

1. SYNCHRONIZATION SPEED
   Instead of validating EVERY block, sync nodes can:
   - Jump to a checkpoint
   - Download blocks after it
   - Save time on verification

2. ATTACK PREVENTION (long-range attacks)
   Attackers need 51% power from the checkpoint height backwards
   Older checkpoints = harder to attack

3. FORK DETECTION
   If two nodes disagree on a checkpoint, they're on different forks
   This helps identify and prevent chain splits

4. NETWORK HEALTH
   Checkpoints show where the network agrees
   Too few = vulnerable
   Too many = mature/stable

CHECKPOINT VALIDATION:
══════════════════════

When IA sees a new block:

1. Check: Does a checkpoint exist for this height?
   If NO  → Block can't be verified here, continue
   If YES → Check next step

2. Validate: Does block hash match checkpoint?
   If MATCH    → Block is valid ✓
   If MISMATCH → Block is invalid ✗ (possible fork)

CHECKPOINT LOADING ORDER (on daemon startup):
═════════════════════════════════════════════

1. Compiled-in checkpoints (hardest to forge, can't be changed)
2. JSON file checkpoints (can be updated, controlled by user)
3. DNS checkpoints (network-provided, trust group consensus)

IA monitors all three sources and detects conflicts.

EXAMPLE CHECKPOINT DATA:
═══════════════════════

Height: 1234570
Hash: "5f8c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"
Difficulty: "987654321098765432109876543210"
Source: "compiled"
Loaded: 2026-01-25 12:34:56
Verified: 12 nodes agree

This tells IA:
✓ At height 1234570, the correct block hash is "5f8c0c..."
✓ Network had this much difficulty accumulated by then
✓ This checkpoint is built into the binary (most trusted)
✓ 12 nodes have confirmed this checkpoint
*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ INTEGRATION EXAMPLE                                                       ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
FULL EXAMPLE OF CHECKPOINT INTEGRATION IN DAEMON:

In src/daemon/daemon.cpp constructor:

    Daemon::Daemon() : m_running(true) {
        
        // 1. Initialize IA FIRST (as always)
        MINFO("[DAEMON] Initializing IA Module...");
        AIModule::getInstance().initialize();
        
        // 2. Initialize checkpoints system
        checkpoints::instance().add_checkpoint(1000000, "hash123");
        // ... add more checkpoints ...
        
        // 3. IA learns about checkpoints
        ia_checkpoint_monitor_initialize();
        ia_set_checkpoint_network("mainnet");
        
        // For each loaded checkpoint:
        ia_register_checkpoint(1000000, "hash123", "difficulty123", "compiled");
        ia_register_checkpoint(2000000, "hash456", "difficulty456", "json");
        
        // 4. Print analysis
        ia_print_checkpoint_status();
        MINFO(ia_get_verification_strategy());
        
        // 5. Continue with rest of daemon startup
        initialize_p2p();
        start_blockchain_sync();
    }

During block validation in blockchain.cpp:

    bool BlockChain::validate_block(const Block& block) {
        
        // First, IA verifies against checkpoints
        if (!ia_verify_block_against_checkpoints(block.height, block.hash)) {
            MERROR("Block failed checkpoint verification");
            return false;
        }
        
        // Then, validate PoW and other properties
        if (!validate_pow(block)) {
            return false;
        }
        
        // Block is valid
        return true;
    }

During peer comparison in p2p.cpp:

    void P2P::verify_peer(const Peer& peer) {
        // ...
        
        // Check if peer has conflicting checkpoints
        uint64_t conflict_height = 0;
        if (ia_detect_checkpoint_fork(peer.checkpoints, conflict_height)) {
            MERROR("Peer is on a different fork at height " << conflict_height);
            peer.set_untrusted();
            return;
        }
        
        // Peer is trustworthy
        peer.set_trusted();
    }

*/

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║ CHECKPOINT MONITORING IN IA MAIN LOOP                                     ║
// ╚════════════════════════════════════════════════════════════════════════════╝

/*
The IA's main monitoring loop (60-second intervals) should include:

In AIModule::monitor_loop():

    void AIModule::monitor_loop() {
        while (running) {
            sleep(60 seconds);
            
            // 1. Verify code integrity (existing)
            verify_code_integrity();
            
            // 2. Monitor network isolation (existing)
            monitor_network_sandbox();
            
            // 3. Check quarantine systems (existing)
            process_quarantine_queue();
            
            // ===== NEW: Checkpoint monitoring =====
            
            // 4. Verify checkpoint integrity
            auto cp_knowledge = ia_get_checkpoint_knowledge();
            if (cp_knowledge.total_checkpoints > 0) {
                MDEBUG("[IA MONITOR] Checkpoint integrity check: "
                       << cp_knowledge.total_checkpoints << " checkpoints active");
            }
            
            // 5. Detect anomalies in checkpoint heights
            if (cp_knowledge.latest_checkpoint_height > expected_height) {
                MWARNING("[IA MONITOR] Checkpoints updated beyond expected height");
                // could indicate fork or stale data
            }
            
            // 6. Log checkpoint statistics
            if (loop_count % 10 == 0) {  // Every 10 minutes
                ia_print_checkpoint_analysis();
            }
        }
    }
*/

#endif // IA_CHECKPOINT_INTEGRATION_GUIDE_H
