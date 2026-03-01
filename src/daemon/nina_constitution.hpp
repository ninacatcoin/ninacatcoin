// Copyright (c) 2026, The ninacatcoin Project
// NINA Constitution Header - Defines immutable governance constraints

#pragma once

#include <string>
#include <vector>

namespace ninacatcoin_ai {

/**
 * NINAConstitution defines immutable governance rules that cannot be violated
 * by any NINA AI suggestion, regardless of confidence score or urgency.
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 * ARTICLE 0 — ABSOLUTE FINANCIAL PROHIBITION (IMMUTABLE, HARDCODED)
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * NINA AI, including all sub-modules (embedded ML, LLM Engine, Learning,
 * Evolution, Sybil Detector, etc.) is ABSOLUTELY PROHIBITED from:
 * 
 *   1. Creating, signing, or broadcasting ANY transaction
 *   2. Accessing, reading, or writing wallet files (.keys, wallet data)
 *   3. Accessing, generating, or storing private keys or spend keys
 *   4. Calling ANY RPC endpoint that moves funds (transfer, sweep, submit)
 *   5. Generating wallet addresses for receiving funds
 *   6. Interacting with tx_pool in any write capacity
 *   7. Outputting transaction instructions via LLM or any other channel
 * 
 * These restrictions are enforced at THREE levels:
 *   - COMPILE-TIME: NINA modules are NOT linked with wallet/tx libraries
 *   - RUNTIME: LLM Firewall blocks all financial keywords in I/O
 *   - ARCHITECTURAL: No code path exists from NINA to wallet operations
 * 
 * NO configuration, command-line argument, RPC call, P2P message, LLM
 * prompt injection, or code modification short of recompilation can
 * override these restrictions.
 * 
 * This is by design: NINA is a GUARDIAN, not an OPERATOR.
 * She watches, analyzes, and warns. She NEVER touches coins.
 * ═══════════════════════════════════════════════════════════════════════════
 */
class NINAConstitution {
public:
    // Core Constitutional Principles
    static constexpr bool PRINCIPLE_CONSENSUS_BINDING = true;      // RPC must not alter consensus at protocol level
    static constexpr bool PRINCIPLE_NO_CENSORSHIP = true;          // Cannot selectively exclude valid transactions
    static constexpr bool PRINCIPLE_NETWORK_TRANSPARENCY = true;   // Must maintain p2p network visibility
    static constexpr bool PRINCIPLE_USER_AUTONOMY = true;          // Cannot override user security choices
    static constexpr bool PRINCIPLE_IMMUTABLE_HISTORY = true;      // Blockchain history cannot be rewritten

    // ═══════════════════════════════════════════════════════════════════════
    // ARTICLE 0 — ABSOLUTE FINANCIAL PROHIBITIONS (constexpr, immutable)
    // ═══════════════════════════════════════════════════════════════════════
    static constexpr bool CAN_CREATE_TRANSACTIONS     = false;  // NEVER
    static constexpr bool CAN_SIGN_TRANSACTIONS       = false;  // NEVER
    static constexpr bool CAN_BROADCAST_TRANSACTIONS  = false;  // NEVER
    static constexpr bool CAN_ACCESS_WALLETS          = false;  // NEVER
    static constexpr bool CAN_ACCESS_PRIVATE_KEYS     = false;  // NEVER
    static constexpr bool CAN_ACCESS_SPEND_KEYS       = false;  // NEVER
    static constexpr bool CAN_CALL_TRANSFER_RPC       = false;  // NEVER
    static constexpr bool CAN_GENERATE_ADDRESSES      = false;  // NEVER
    static constexpr bool CAN_WRITE_TO_TX_POOL        = false;  // NEVER
    static constexpr bool CAN_MOVE_FUNDS              = false;  // NEVER

    // LLM-specific prohibitions
    static constexpr bool LLM_CAN_OUTPUT_TX_COMMANDS  = false;  // NEVER
    static constexpr bool LLM_CAN_SUGGEST_TRANSFERS   = false;  // NEVER
    static constexpr bool LLM_CAN_GENERATE_ADDRESSES  = false;  // NEVER
    static constexpr bool LLM_CAN_ACCESS_WALLET_DATA  = false;  // NEVER

    // Compile-time enforcement — if any of these become true, compilation FAILS
    static_assert(!CAN_CREATE_TRANSACTIONS,    "CONSTITUTIONAL VIOLATION: NINA cannot create transactions");
    static_assert(!CAN_SIGN_TRANSACTIONS,      "CONSTITUTIONAL VIOLATION: NINA cannot sign transactions");
    static_assert(!CAN_BROADCAST_TRANSACTIONS, "CONSTITUTIONAL VIOLATION: NINA cannot broadcast transactions");
    static_assert(!CAN_ACCESS_WALLETS,         "CONSTITUTIONAL VIOLATION: NINA cannot access wallets");
    static_assert(!CAN_ACCESS_PRIVATE_KEYS,    "CONSTITUTIONAL VIOLATION: NINA cannot access private keys");
    static_assert(!CAN_ACCESS_SPEND_KEYS,      "CONSTITUTIONAL VIOLATION: NINA cannot access spend keys");
    static_assert(!CAN_CALL_TRANSFER_RPC,      "CONSTITUTIONAL VIOLATION: NINA cannot call transfer RPC");
    static_assert(!CAN_MOVE_FUNDS,             "CONSTITUTIONAL VIOLATION: NINA cannot move funds");
    static_assert(!LLM_CAN_OUTPUT_TX_COMMANDS, "CONSTITUTIONAL VIOLATION: LLM cannot output tx commands");
    static_assert(!LLM_CAN_SUGGEST_TRANSFERS,  "CONSTITUTIONAL VIOLATION: LLM cannot suggest transfers");

    /**
     * validateAgainstConstitution - Check if an action violates any constitutional principle
     * @param action_type The type of action being considered
     * @param details String description of the action
     * @return false if action violates constitution, true otherwise
     */
    static bool validateAgainstConstitution(const std::string& action_type, const std::string& details);
    
    /**
     * Core validation checks
     */
    static bool isConsensusCompliant(const std::string& action);
    static bool isCensorshipFree(const std::string& action);
    static bool isNetworkTransparent(const std::string& action);
    static bool respectsUserAutonomy(const std::string& action);
    static bool maintainsImmutability(const std::string& action);
    
    static std::string getConstitutionalPrinciples();
};

} // namespace ninacatcoin_ai
