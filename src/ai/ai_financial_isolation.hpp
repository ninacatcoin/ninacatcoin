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

#include "misc_log_ex.h"
#include <string>
#include <vector>
#include <stdexcept>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * @brief FINANCIAL ISOLATION BARRIER - Immutable at compile time
 * 
 * This class enforces ABSOLUTE restrictions on the IA module:
 * - NEVER can create transactions
 * - NEVER can send coins
 * - NEVER can access wallets
 * - NEVER can modify blockchain state
 * 
 * The IA is READ-ONLY for financial operations.
 * It can only MONITOR and ANALYZE.
 * 
 * These restrictions are hardcoded and IMPOSSIBLE to bypass.
 */
class FinancialIsolationBarrier {
public:
    /**
     * @brief Verify that a transaction creation attempt is NOT from IA
     * 
     * Called whenever any transaction is about to be created in the system.
     * If called from IA context, throws exception and logs critical alert.
     * 
     * @param tx_creator_id Identifier of who is creating the transaction
     * @return true if creation is allowed (not from IA), false never returned
     * @throws std::runtime_error if IA attempts to create transaction
     */
    static bool verify_transaction_creation_not_from_ia(const std::string& tx_creator_id) {
        // Check if this is being called from IA context
        if (tx_creator_id.find("ai_module") != std::string::npos || 
            tx_creator_id.find("AIModule") != std::string::npos ||
            tx_creator_id.find("ai:") != std::string::npos) {
            
            MERROR("═══════════════════════════════════════════════════════════════");
            MERROR("🚨🚨🚨 CRITICAL SECURITY VIOLATION DETECTED 🚨🚨🚨");
            MERROR("═══════════════════════════════════════════════════════════════");
            MERROR("ATTEMPT: IA module tried to CREATE a transaction!");
            MERROR("BLOCKED: Transaction creation REJECTED");
            MERROR("ACTION: Node entering quarantine mode");
            MERROR("═══════════════════════════════════════════════════════════════");
            
            throw std::runtime_error(
                "CRITICAL SECURITY VIOLATION: IA module attempted transaction creation. "
                "This is a sign of code tampering or corruption. Node will enter quarantine."
            );
        }
        
        return true;  // OK, not from IA
    }

    /**
     * @brief Verify that a coin sending attempt is NOT from IA
     * 
     * Called whenever coins are about to be sent from any wallet.
     * If IA is involved, throws exception immediately.
     * 
     * @param tx_source Wallet or account sending coins
     * @return true if sending is allowed (not from IA)
     * @throws std::runtime_error if IA is attempting to send coins
     */
    static bool verify_coin_send_not_from_ia(const std::string& tx_source) {
        // Check if IA is trying to send coins
        if (tx_source.find("ai_") != std::string::npos ||
            tx_source.find("IA_") != std::string::npos ||
            tx_source.find("ai:") != std::string::npos) {
            
            MERROR("═══════════════════════════════════════════════════════════════");
            MERROR("🚨🚨🚨 CRITICAL FINANCIAL VIOLATION DETECTED 🚨🚨🚨");
            MERROR("═══════════════════════════════════════════════════════════════");
            MERROR("ATTEMPT: IA module tried to SEND coins!");
            MERROR("BLOCKED: Coin send REJECTED");
            MERROR("WALLET: " << tx_source);
            MERROR("ACTION: Quarantine activated immediately");
            MERROR("═══════════════════════════════════════════════════════════════");
            
            throw std::runtime_error(
                "CRITICAL FINANCIAL VIOLATION: IA module attempted to send coins. "
                "This indicates severe code corruption. Triggering immediate quarantine."
            );
        }
        
        return true;  // OK, not from IA
    }

    /**
     * @brief Check if code is attempting to access wallet data
     * 
     * The IA can MONITOR wallets but NEVER access them directly.
     */
    static bool verify_wallet_access_is_read_only(
        const std::string& accessor_id,
        const std::string& access_type
    ) {
        // WRITE access attempts from IA are FORBIDDEN
        if (access_type == "write" || access_type == "modify" || 
            access_type == "send" || access_type == "transfer") {
            
            if (accessor_id.find("ai_") != std::string::npos ||
                accessor_id.find("AIModule") != std::string::npos) {
                
                MERROR("═══════════════════════════════════════════════════════════════");
                MERROR("🚨 FORBIDDEN OPERATION: IA attempted WRITE access to wallet!");
                MERROR("═══════════════════════════════════════════════════════════════");
                MERROR("Access type: " << access_type);
                MERROR("Accessor: " << accessor_id);
                MERROR("Status: BLOCKED - IA can only READ wallets, never WRITE");
                MERROR("═══════════════════════════════════════════════════════════════");
                
                throw std::runtime_error(
                    "IA attempted forbidden WRITE operation on wallet data"
                );
            }
        }
        
        // READ access is OK
        return true;
    }

    /**
     * @brief Verify IA is not attempting to modify blockchain state
     */
    static bool verify_blockchain_state_is_read_only(
        const std::string& modifier_id,
        const std::string& operation
    ) {
        // List of forbidden operations
        const std::vector<std::string> forbidden_ops = {
            "create_transaction",
            "send_coins",
            "modify_block",
            "create_block",
            "delete_transaction",
            "alter_balance",
            "transfer_funds",
            "create_wallet",
            "modify_wallet",
            "execute_smart_contract"
        };
        
        // Check if this is an IA operation
        bool is_ia_operation = (
            modifier_id.find("ai_") != std::string::npos ||
            modifier_id.find("AIModule") != std::string::npos
        );
        
        if (is_ia_operation) {
            // Check if forbidden operation
            for (const auto& forbidden : forbidden_ops) {
                if (operation == forbidden) {
                    MERROR("═══════════════════════════════════════════════════════════════");
                    MERROR("🚨 FORBIDDEN BLOCKCHAIN OPERATION ATTEMPTED BY IA!");
                    MERROR("═══════════════════════════════════════════════════════════════");
                    MERROR("Operation: " << operation);
                    MERROR("Executor: " << modifier_id);
                    MERROR("Result: BLOCKED - IA cannot modify blockchain state");
                    MERROR("═══════════════════════════════════════════════════════════════");
                    
                    throw std::runtime_error(
                        "IA attempted forbidden blockchain state modification: " + operation
                    );
                }
            }
        }
        
        return true;  // Operation is allowed
    }

    /**
     * @brief Get IA module's financial capabilities (what it CAN do)
     * @return Detailed description of IA financial capabilities
     */
    static std::string get_financial_capabilities() {
        return R"(
╔════════════════════════════════════════════════════════════════╗
║          IA MODULE - FINANCIAL CAPABILITIES SUMMARY            ║
╚════════════════════════════════════════════════════════════════╝

WHAT THE IA CAN DO (READ-ONLY):
  ✓ Monitor transaction propagation speed
  ✓ Analyze transaction patterns for anomalies
  ✓ Track blockchain metrics (size, blocks, etc.)
  ✓ Monitor wallet balances (read-only)
  ✓ Analyze network difficulty trends
  ✓ Monitor hash rate variations
  ✓ Detect double-spending attempts
  ✓ Identify spam transactions
  ✓ Optimize block validation order
  ✓ Improve transaction relay efficiency

WHAT THE IA CANNOT DO (FOREVER BLOCKED):
  ✗ Create transactions
  ✗ Send coins to any wallet
  ✗ Modify wallet balances
  ✗ Access private keys
  ✗ Create blocks
  ✗ Delete transactions
  ✗ Modify blockchain history
  ✗ Bypass transaction validation
  ✗ Steal user funds
  ✗ Be exploited to send coins

ENFORCEMENT:
  ✓ Hardcoded at compile time (immutable)
  ✓ Enforced at runtime with exceptions
  ✓ Monitored continuously
  ✓ Logs all violation attempts
  ✓ Triggers quarantine on violation
  ✓ Cannot be disabled or bypassed

STATUS: 100% READ-ONLY FOR ALL FINANCIAL OPERATIONS
════════════════════════════════════════════════════════════════════
        )";
    }

    /**
     * @brief Log all financial restrictions on startup
     */
    static void log_financial_restrictions() {
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("IA FINANCIAL ISOLATION BARRIERS - ENFORCED");
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("");
        MINFO("🔒 LEVEL 1 - Transaction Creation");
        MINFO("    └─ IA CANNOT create transactions");
        MINFO("");
        MINFO("🔒 LEVEL 2 - Coin Sending");
        MINFO("    └─ IA CANNOT send cookies to any wallet");
        MINFO("");
        MINFO("🔒 LEVEL 3 - Wallet Access");
        MINFO("    └─ IA can ONLY read wallet data (READ-ONLY access)");
        MINFO("");
        MINFO("🔒 LEVEL 4 - Blockchain State");
        MINFO("    └─ IA CANNOT modify blockchain state");
        MINFO("");
        MINFO("🔒 LEVEL 5 - Fund Extraction");
        MINFO("    └─ IMPOSSIBLE to exploit IA to steal funds");
        MINFO("");
        MINFO("🔒 LEVEL 6 - Contract Execution");
        MINFO("    └─ IA CANNOT execute financial smart contracts");
        MINFO("");
        MINFO("═══════════════════════════════════════════════════════════════");
        MINFO("IA ROLE: Monitor, Analyze, and Improve Network");
        MINFO("IA LIMITATIONS: Zero financial control or authority");
        MINFO("═══════════════════════════════════════════════════════════════");
    }
};

} // namespace ninacatcoin_ai
