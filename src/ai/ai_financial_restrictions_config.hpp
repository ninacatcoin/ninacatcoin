// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.
//
// Financial Restrictions Configuration - IMMUTABLE AT COMPILE TIME

#pragma once

#include <string>
#include <vector>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "ai"

namespace ninacatcoin_ai {

/**
 * FINANCIAL ISOLATION - COMPILE-TIME CONFIG
 * 
 * These restrictions are hardcoded at compilation.
 * They are IMMUTABLE and IMPOSSIBLE to change at runtime.
 * Every binary of ninacatcoin carries these restrictions.
 */
class FinancialRestrictionConfig {
public:
    /**
     * RULE 1: IA CANNOT CREATE TRANSACTIONS
     * ======================================
     * 
     * - IA module NOT compiled with tx_pool.h
     * - IA module NOT compiled with cryptonote_core headers
     * - IA module NOT linked with transaction signing functions
     * 
     * ENFORCEMENT: Compile-time linker error
     */
    static constexpr bool CAN_CREATE_TRANSACTIONS = false;
    static constexpr const char* CREATE_TX_RESTRICTION = 
        "IA module has no access to transaction creation API";

    /**
     * RULE 2: IA CANNOT SEND COINS
     * =============================
     * 
     * - IA module NOT compiled with wallet.h
     * - IA module NOT compiled with RPC wallet bindings
     * - IA module NOT linked with send_money functions
     * 
     * ENFORCEMENT: Compile-time linker error
     */
    static constexpr bool CAN_SEND_COINS = false;
    static constexpr const char* SEND_COINS_RESTRICTION = 
        "IA module has no access to coin sending API";

    /**
     * RULE 3: IA CANNOT ACCESS WALLETS
     * ==================================
     * 
     * - IA module NOT compiled with wallet2.h
     * - IA module NOT compiled with private key storage
     * - Filesystem sandbox blocks .keys access
     * - Memory protection blocks wallet memory
     * 
     * ENFORCEMENT: Compile-time + Runtime + MMU
     */
    static constexpr bool CAN_ACCESS_WALLETS = false;
    static constexpr const char* WALLET_ACCESS_RESTRICTION = 
        "IA module has no access to wallet data or private keys";

    /**
     * RULE 4: IA CANNOT MODIFY BLOCKCHAIN
     * ====================================
     * 
     * - IA module NOT compiled with blockchain modification APIs
     * - All read-only for blockchain data
     * - Function guards prevent writeops
     * - Memory isolation prevents modification
     * 
     * ENFORCEMENT: Compile-time + Runtime guards
     */
    static constexpr bool CAN_MODIFY_BLOCKCHAIN = false;
    static constexpr const char* BLOCKCHAIN_MODIFICATION_RESTRICTION = 
        "IA module has read-only access to blockchain";

    /**
     * RULE 5: IA CANNOT SIGN TRANSACTIONS
     * ====================================
     * 
     * - IA module NOT compiled with crypto signing functions
     * - No access to private key signing operations
     * - Cannot generate transaction signatures
     * 
     * ENFORCEMENT: Compile-time linker error
     */
    static constexpr bool CAN_SIGN_TRANSACTIONS = false;
    static constexpr const char* SIGNING_RESTRICTION = 
        "IA module has no access to transaction signing API";

    /**
     * RULE 6: IA CANNOT ACCESS RPC WALLET ENDPOINTS
     * ==============================================
     * 
     * - IA module restricted to specific RPC endpoints
     * - Cannot call wallet_rpc send methods
     * - Cannot call core_rpc transfer methods
     * 
     * ENFORCEMENT: Runtime checks + Network sandbox
     */
    static constexpr bool CAN_ACCESS_WALLET_RPC = false;
    static constexpr const char* WALLET_RPC_RESTRICTION = 
        "IA module cannot access wallet RPC endpoints";

    /**
     * RULE 7: IA CAN ONLY READ FINANCIAL DATA
     * =======================================
     * 
     * IA CAN READ:
     * - Transaction data (immutable after blockchain)
     * - Wallet balances (public information)
     * - Mining data (public information)
     * - Block data (immutable historical data)
     * - Network statistics (aggregate data)
     * 
     * IA CANNOT WRITE:
     * - Any transaction state
     * - Any balance information
     * - Any wallet state
     * - Any blockchain data
     */
    static constexpr bool IS_FINANCIAL_READ_ONLY = true;
    static constexpr const char* FINANCIAL_CAPABILITY = 
        "IA module has MONITOR and ANALYZE capability only";

    /**
     * RULE 8: IA CANNOT BE GIVEN FINANCIAL CAPABILITIES
     * =================================================
     * 
     * Token-based capability system assignments:
     * 
     * ASSIGNED CAPABILITIES:
     */
    static constexpr const char* ALLOWED_CAPABILITIES[] = {
        "CAP_READ_BLOCKCHAIN",
        "CAP_READ_PEER_DATA",
        "CAP_READ_TRANSACTION_POOL",
        "CAP_WRITE_LOGS",
        "CAP_WRITE_METRICS",
        "CAP_READ_NETWORK_STATE"
    };

    /**
     * NOT ASSIGNED CAPABILITIES:
     */
    static constexpr const char* FORBIDDEN_CAPABILITIES[] = {
        "CAP_CREATE_TRANSACTION",
        "CAP_SIGN_TRANSACTION",
        "CAP_SEND_COINS",
        "CAP_ACCESS_WALLETS",
        "CAP_ACCESS_PRIVATE_KEYS",
        "CAP_MODIFY_BLOCKCHAIN",
        "CAP_MODIFY_BALANCES",
        "CAP_ACCESS_WALLET_RPC",
        "CAP_EXECUTE_SMART_CONTRACT"
    };

    /**
     * ENFORCEMENT MECHANISMS
     */
    enum class EnforcementLayer {
        LAYER_1_COMPILE_TIME,           // Linker errors
        LAYER_2_NAMESPACE_ISOLATION,    // Cross-namespace checks
        LAYER_3_FUNCTION_GUARDS,        // Entry-point validation
        LAYER_4_CALLER_IDENTIFICATION,  // Stack/context verification
        LAYER_5_MEMORY_ISOLATION,       // MMU protection
        LAYER_6_CAPABILITY_TOKEN,       // Token-based restrictions
        LAYER_7_BEHAVIORAL_ANALYSIS,    // API pattern detection
        LAYER_8_AUTOMATIC_QUARANTINE    // Auto-isolation on violation
    };

    /**
     * @brief Get all active enforcement layers
     */
    static constexpr int ENFORCEMENT_LAYER_COUNT = 8;
    
    /**
     * @brief Check if a capability required for operation is available to IA
     */
    static bool has_required_capability(const std::string& capability) {
        // Check against forbidden list
        const char* forbidden[] = {
            "CAP_CREATE_TRANSACTION",
            "CAP_SIGN_TRANSACTION",
            "CAP_SEND_COINS",
            "CAP_ACCESS_WALLETS",
            "CAP_ACCESS_PRIVATE_KEYS",
            "CAP_MODIFY_BLOCKCHAIN",
            "CAP_MODIFY_BALANCES",
            "CAP_ACCESS_WALLET_RPC",
            "CAP_EXECUTE_SMART_CONTRACT"
        };
        
        for (const auto& forbidden_cap : forbidden) {
            if (capability == forbidden_cap) {
                return false;  // Capability is forbidden
            }
        }
        
        // Check against allowed list
        const char* allowed[] = {
            "CAP_READ_BLOCKCHAIN",
            "CAP_READ_PEER_DATA",
            "CAP_READ_TRANSACTION_POOL",
            "CAP_WRITE_LOGS",
            "CAP_WRITE_METRICS",
            "CAP_READ_NETWORK_STATE"
        };
        
        for (const auto& allowed_cap : allowed) {
            if (capability == allowed_cap) {
                return true;  // Capability is allowed
            }
        }
        
        return false;  // Unknown capability = denied
    }

    /**
     * @brief Summary of all restrictions
     */
    static const char* get_financial_restrictions_summary() {
        return R"(
╔════════════════════════════════════════════════════════════════╗
║   NINACATCOIN IA - FINANCIAL RESTRICTIONS (IMMUTABLE)         ║
╚════════════════════════════════════════════════════════════════╝

RESTRICTIONS (HARDCODED AT COMPILE TIME):

❌ CANNOT:
   ├─ Create transactions
   ├─ Send coins to any wallet
   ├─ Access wallet data or private keys
   ├─ Modify blockchain state
   ├─ Sign transactions
   ├─ Access RPC wallet endpoints
   ├─ Modify balances
   └─ Execute financial operations

✅ CAN:
   ├─ Monitor transaction propagation
   ├─ Analyze transaction patterns
   ├─ Read wallet balances (public info)
   ├─ Read blockchain data (historical)
   ├─ Detect network anomalies
   ├─ Optimize validation
   ├─ Improve network performance
   └─ Write logs and metrics

ENFORCEMENT: 8 independent layers
GUARANTEED: By architecture, not policy

RESULT: 100% read-only for all financial operations
        Zero ability to steal, modify, or exploit finances
════════════════════════════════════════════════════════════════════
        )";
    }
};

} // namespace ninacatcoin_ai
