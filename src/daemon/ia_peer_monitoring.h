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
#include <string>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "network"

namespace daemonize {

/**
 * @brief IA Monitoring integration for P2P network operations
 * 
 * The IA module monitors network activity to:
 * - Validate incoming peers
 * - Detect suspicious connection patterns
 * - Monitor transaction propagation
 * - Identify potential attacks
 * - Optimize network routing
 */
class IAPeerMonitoring {
public:
    /**
     * @brief Called when a new peer connects to the network
     * @param peer_id Unique identifier for the peer
     * @param peer_address IP address of the peer
     * @return true if peer is acceptable, false if should be blocked
     */
    static bool on_peer_connected(
        const std::string& peer_id,
        const std::string& peer_address
    ) {
        try {
            // Check if peer is blacklisted
            if (ninacatcoin_ai::AIModule::getInstance().isPeerBlacklisted(peer_id)) {
                MWARNING("[IA Network] Peer " << peer_id << " is BLACKLISTED, rejecting connection");
                return false;
            }
            
            // Register with IA for monitoring
            ninacatcoin_ai::AIModule::getInstance().registerPeer(peer_id, peer_address);
            
            MDEBUG("[IA Network] Peer " << peer_id << " registered with IA security module");
            return true;
        }
        catch (const std::exception& e) {
            MERROR("[IA Network] Exception in peer validation: " << e.what());
            return true; // Don't block on exception, continue normally
        }
    }

    /**
     * @brief Called when a peer disconnects from the network
     * @param peer_id Unique identifier for the peer
     */
    static void on_peer_disconnected(const std::string& peer_id) {
        try {
            MDEBUG("[IA Network] Peer " << peer_id << " disconnected");
            // IA module tracks this internally
        }
        catch (const std::exception& e) {
            MERROR("[IA Network] Exception in peer disconnect: " << e.what());
        }
    }

    /**
     * @brief Called when receiving a transaction from network
     * @param tx_id Transaction ID
     * @param tx_size Size of transaction in bytes
     * @return true if transaction should be processed, false to reject it
     */
    static bool on_transaction_received(
        const std::string& tx_id,
        uint64_t tx_size
    ) {
        try {
            bool approved = ninacatcoin_ai::AIModule::getInstance().analyzeTransaction(
                tx_id,
                tx_size
            );
            
            if (!approved) {
                MWARNING("[IA Network] Transaction " << tx_id << " flagged by IA analysis");
                return false;
            }
            
            return true;
        }
        catch (const std::exception& e) {
            MERROR("[IA Network] Exception analyzing transaction: " << e.what());
            return true; // Don't block on analysis error
        }
    }

    /**
     * @brief Called periodically for network health monitoring
     * Should be called every 30-60 seconds
     */
    static void on_network_heartbeat() {
        try {
            // IA module performs internal monitoring
            // This is mostly for logging and explicit health checks
            //MDEBUG("[IA Network] Network heartbeat - IA module monitoring active");
        }
        catch (const std::exception& e) {
            MERROR("[IA Network] Exception in network heartbeat: " << e.what());
        }
    }

    /**
     * @brief Get current network status from IA perspective
     * @return JSON string with network stats
     */
    static std::string get_network_status() {
        try {
            return ninacatcoin_ai::AIModule::getInstance().getStatus();
        }
        catch (const std::exception& e) {
            return "{\"error\": \"" + std::string(e.what()) + "\"}";
        }
    }

    /**
     * @brief Log detailed network monitoring information
     * Called for detailed diagnostics
     */
    static void log_network_diagnostics() {
        try {
            MINFO("═══════════════════════════════════════════════════════════════");
            MINFO("IA Network Monitoring Diagnostics:");
            MINFO("═══════════════════════════════════════════════════════════════");
            MINFO(get_network_status());
            MINFO("═══════════════════════════════════════════════════════════════");
        }
        catch (const std::exception& e) {
            MERROR("Exception getting network diagnostics: " << e.what());
        }
    }
};

} // namespace daemonize
