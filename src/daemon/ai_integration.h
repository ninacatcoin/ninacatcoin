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
     * @brief Shutdown IA module gracefully
     */
    static void shutdown_ia_module()
    {
        try {
            MINFO("[IA] Shutting down IA Security Module...");
            ninacatcoin_ai::AIModule::getInstance().shutdown();
            MINFO("[IA] IA module shutdown complete");
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
