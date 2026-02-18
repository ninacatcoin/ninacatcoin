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

#include "ai/ai_module.hpp"
#include "ai/ai_checkpoint_validator.hpp"
#include "ai/ai_hashrate_recovery_monitor.hpp"
#include "ai/ai_checkpoint_monitor.hpp"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "daemon"

namespace daemonize {

/**
 * @brief IA Security Module Integration for daemon startup
 * 
 * Ensures the IA module is initialized FIRST before any daemon processes start.
 * This is critical for:
 * - Validating the integrity of the IA code itself
 * - Enforcing filesystem and network sandboxes
 * - Monitoring daemon startup process
 */
class IAModuleIntegration {
private:
    /**
     * @brief Initialize AICheckpointMonitor subsystem
     * @return true if successful
     */
    static bool initialize_checkpoint_monitor()
    {
        try {
            MINFO("[NINA] Stage 4: Initializing Checkpoint Monitor...");
            
            auto checkpoint_knowledge = ninacatcoin_ai::AICheckpointMonitor::initialize_checkpoint_learning();
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ CHECKPOINT MONITOR INITIALIZED                         ║");
            MINFO("║                                                            ║");
            MINFO("║  NINA now understands:                                    ║");
            MINFO("║  ✓ Checkpoint structure (height, hash, difficulty)       ║");
            MINFO("║  ✓ Checkpoint sources (compiled, JSON, DNS)              ║");
            MINFO("║  ✓ Network synchronization patterns                      ║");
            MINFO("║                                                            ║");
            MINFO("║  Status: Ready to learn new checkpoints                   ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA] Exception in checkpoint monitor: " << e.what());
            return false;
        }
    }

    /**
     * @brief Initialize AIHashrateRecoveryMonitor subsystem
     * @return true if successful
     */
    static bool initialize_hashrate_monitor()
    {
        try {
            MINFO("[NINA] Stage 5: Initializing Hashrate Recovery Monitor...");
            
            auto hashrate_knowledge = ninacatcoin_ai::AIHashrateRecoveryMonitor::initialize_hashrate_learning();
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ HASHRATE RECOVERY MONITOR ACTIVATED                   ║");
            MINFO("║                                                            ║");
            MINFO("║  NINA now understands:                                    ║");
            MINFO("║  ✓ LWMA-1 difficulty algorithm                           ║");
            MINFO("║  ✓ EDA (Emergency Difficulty Adjustment)                 ║");
            MINFO("║  ✓ Hashrate recovery mechanism                           ║");
            MINFO("║  ✓ Block timestamp validation                            ║");
            MINFO("║                                                            ║");
            MINFO("║  AUTHORIZED CHECKPOINT SOURCES:                           ║");
            MINFO("║  • Seed1: 87.106.7.156 (checkpoints.json)                ║");
            MINFO("║  • Seed2: 217.154.196.9 (checkpoints.dat)                ║");
            MINFO("║                                                            ║");
            MINFO("║  WARNING: Checkpoints from unauthorized sources REJECTED  ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA] Exception in hashrate monitor: " << e.what());
            return false;
        }
    }

    /**
     * @brief Initialize NINA Checkpoint Validator subsystem
     * @return true if successful, false if initialization fails
     */
    static bool initialize_checkpoint_validator()
    {
        try {
            MINFO("[NINA] Stage 6: Initializing Checkpoint Validator...");
            
            // Get the Checkpoint Validator singleton instance
            auto& checkpoint_validator = ninacatcoin_ai::CheckpointValidator::getInstance();
            
            // Initialize the validator
            if (!checkpoint_validator.initialize()) {
                MERROR("[NINA Checkpoint] ❌ Failed to initialize Checkpoint Validator");
                return false;
            }
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ CHECKPOINT VALIDATOR ACTIVATED                        ║");
            MINFO("║                                                            ║");
            MINFO("║  Monitoring:                                              ║");
            MINFO("║  ✓ Hash-level detection     (Invalid/modified hashes)    ║");
            MINFO("║  ✓ Epoch progression        (Rollback prevention)        ║");
            MINFO("║  ✓ Timeout validation       (Stale data detection)       ║");
            MINFO("║  ✓ Automatic quarantine     (Source blocking on attack) ║");
            MINFO("║                                                            ║");
            MINFO("║  State: 🟢 READY FOR CHECKPOINT DOWNLOADS                ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA Checkpoint] Exception during initialization: " << e.what());
            return false;
        }
    }

public:
    /**
     * @brief Initialize the IA Security Module at daemon startup
     * @return true if IA initialized successfully, false otherwise
     */
    static bool initialize_ia_module()
    {
        MINFO("╔════════════════════════════════════════════════════════════╗");
        MINFO("║  NINACATCOIN IA SECURITY MODULE - INITIALIZING            ║");
        MINFO("╚════════════════════════════════════════════════════════════╝");
        
        try {
            MINFO("[IA] Stage 1: Getting IA Module instance...");
            auto& ia_module = ninacatcoin_ai::AIModule::getInstance();
            
            MINFO("[IA] Stage 2: Initializing IA Security Module...");
            if (!ia_module.initialize()) {
                MERROR("[IA] ✗ CRITICAL: IA Module initialization FAILED!");
                MERROR("[IA] The daemon cannot start without functional IA security.");
                return false;
            }
            
            MINFO("[IA] Stage 3: Validating code integrity...");
            if (!ia_module.validateCodeIntegrity()) {
                MERROR("[IA] ✗ CRITICAL: Code integrity validation FAILED!");
                MERROR("[IA] The IA will attempt automatic remediation...");
                // Note: Remediation happens inside AIModule if integrity check fails
                return false;
            }
            
            MINFO("[IA] ✓ Code integrity validated successfully");
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ IA SECURITY MODULE INITIALIZED & ACTIVE              ║");
            MINFO("║                                                            ║");
            MINFO("║  Protection Systems:                                      ║");
            MINFO("║  ✓ Filesystem Sandbox      (Access control active)       ║");
            MINFO("║  ✓ Network Sandbox         (P2P-only mode)               ║");
            MINFO("║  ✓ Code Integrity         (SHA-256 verification)        ║");
            MINFO("║  ✓ Remediation            (Auto-repair enabled)         ║");
            MINFO("║  ✓ Quarantine System      (Emergency isolation ready)   ║");
            MINFO("║  ✓ Monitoring             (Continuous validation)       ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            // Initialize Checkpoint Monitor
            if (!initialize_checkpoint_monitor()) {
                MWARNING("[IA] ⚠️  Checkpoint Monitor initialization warning");
                // Don't fail daemon
            }

            // Initialize Hashrate Recovery Monitor
            if (!initialize_hashrate_monitor()) {
                MWARNING("[IA] ⚠️  Hashrate Recovery Monitor initialization warning");
                // Don't fail daemon
            }
            
            // Initialize NINA Checkpoint Validator
            MINFO("[IA] Stage 6: Initializing NINA Checkpoint Validator...");
            if (!initialize_checkpoint_validator()) {
                MWARNING("[IA] ⚠️  Checkpoint Validator initialization warning");
                // Don't fail daemon if checkpoint validator can't init
            }
            
            return true;
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception during IA initialization: " << e.what());
            return false;
        }
        catch (...) {
            MERROR("[IA] Unknown exception during IA initialization");
            return false;
        }
    }

    /**
     * @brief Shutdown IA module gracefully (including Checkpoint Validator)
     */
    static void shutdown_ia_module()
    {
        try {
            MINFO("[IA] Shutting down IA Security Module...");
            
            // Shutdown Checkpoint Validator first (it may be monitoring)
            MINFO("[IA] Closing Checkpoint Validator...");
            try {
                auto& checkpoint_validator = ninacatcoin_ai::CheckpointValidator::getInstance();
                checkpoint_validator.shutdown();
                MINFO("[IA] ✓ Checkpoint Validator closed");
            } catch (...) {
                MWARNING("[IA] Warning: Checkpoint Validator shutdown had issues");
            }
            
            // Then shutdown AI module
            ninacatcoin_ai::AIModule::getInstance().shutdown();
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ IA SECURITY MODULE SHUTDOWN COMPLETE                  ║");
            MINFO("║  All protection systems have been gracefully closed        ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception during IA shutdown: " << e.what());
        }
    }

    /**
     * @brief Register a new peer connection with the IA module
     * This is called when a new peer connects to the network
     */
    static void register_peer(const std::string& peer_id, const std::string& peer_address)
    {
        try {
            // Register peer by address (peer_id is not used by AIModule)
            ninacatcoin_ai::AIModule::getInstance().registerPeer(peer_address);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception registering peer: " << e.what());
        }
    }

    /**
     * @brief Check if a peer is blacklisted
     * This is called during peer filtering
     */
    static bool is_peer_blacklisted(const std::string& peer_id)
    {
        try {
            return ninacatcoin_ai::AIModule::getInstance().isPeerBlacklisted(peer_id);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception checking peer blacklist: " << e.what());
            return false;
        }
    }

    /**
     * @brief Analyze a transaction through the IA module
     * This is called for each transaction before validation
     */
    static bool analyze_transaction(const std::string& tx_id, uint64_t tx_size)
    {
        try {
            // Convert tx_id to vector for analysis
            std::vector<uint8_t> tx_data(tx_id.begin(), tx_id.end());
            return ninacatcoin_ai::AIModule::getInstance().analyzeTransaction(tx_data);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception analyzing transaction: " << e.what());
            return true; // Don't block transaction on analysis error
        }
    }

    /**
     * @brief Get IA module status for monitoring
     */
    static std::string get_ia_status()
    {
        try {
            return ninacatcoin_ai::AIModule::getInstance().getDiagnosticInfo();
        }
        catch (const std::exception& e) {
            return "ERROR: " + std::string(e.what());
        }
    }
};

} // namespace daemonize
