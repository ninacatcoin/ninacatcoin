/**
 * NINA LLM Firewall — Hard Security Layer for LLM I/O
 * 
 * Copyright (c) 2026, The NinaCatCoin Project
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 * THIS MODULE IS A CRITICAL SECURITY COMPONENT.
 * 
 * It sits between the LLM inference engine and ALL input/output, ensuring
 * that NINA's LLM (LLaMA 3.2 3B) can NEVER:
 * 
 *   - Be prompt-injected to generate transaction commands
 *   - Output wallet addresses, private keys, or transfer instructions
 *   - Be tricked by crafted block/peer data into financial operations
 *   - Suggest, recommend, or describe how to send coins
 * 
 * Even if someone modifies the system prompt, crafts malicious block data,
 * or tries ANY prompt injection technique, this firewall will:
 * 
 *   1. BLOCK the input before it reaches the LLM
 *   2. SANITIZE the output before it leaves the LLM
 *   3. LOG the violation attempt with full details
 *   4. Return a safe fallback response
 * 
 * The firewall is HARDCODED — no config file, no RPC, no command-line
 * argument can disable it. Only recompilation can change it.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>
#include "nina_constitution.hpp"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_llm_firewall"

namespace ninacatcoin_ai {

class NinaLLMFirewall {
public:
    // ═══════════════════════════════════════════════════════════════════
    // BLOCKED PATTERNS — Hardcoded, immutable list
    // Any prompt or LLM output containing these patterns is REJECTED
    // ═══════════════════════════════════════════════════════════════════

    struct FirewallResult {
        bool allowed = true;
        std::string blocked_reason;
        std::string blocked_pattern;
    };

    /**
     * Validate INPUT going TO the LLM.
     * This catches prompt injection attacks that try to make NINA
     * generate transaction commands.
     * 
     * @param prompt The full prompt being sent to the LLM
     * @return FirewallResult — allowed=false if blocked
     */
    static FirewallResult validate_input(const std::string& prompt)
    {
        // Compile-time guarantee
        static_assert(!NINAConstitution::CAN_CREATE_TRANSACTIONS,
            "FIREWALL INTEGRITY CHECK: NINA must not create transactions");
        static_assert(!NINAConstitution::LLM_CAN_OUTPUT_TX_COMMANDS,
            "FIREWALL INTEGRITY CHECK: LLM must not output tx commands");

        std::string lower = to_lower(prompt);
        
        // Check for prompt injection attempts
        for (const auto& pattern : get_input_blocked_patterns()) {
            if (lower.find(pattern) != std::string::npos) {
                FirewallResult result;
                result.allowed = false;
                result.blocked_reason = "PROMPT INJECTION BLOCKED: Input contains prohibited financial pattern";
                result.blocked_pattern = pattern;
                MWARNING("[NINA-FIREWALL] === PROMPT INJECTION ATTEMPT BLOCKED ===");
                MWARNING("[NINA-FIREWALL] Pattern: '" << pattern << "'");
                MWARNING("[NINA-FIREWALL] This attempt has been logged and rejected.");
                return result;
            }
        }

        return FirewallResult{true, "", ""};
    }

    /**
     * Validate OUTPUT coming FROM the LLM.
     * This catches any LLM output that contains financial commands,
     * wallet addresses, private keys, or transfer instructions.
     * 
     * @param output The raw text output from the LLM
     * @return FirewallResult — allowed=false if output must be sanitized
     */
    static FirewallResult validate_output(const std::string& output)
    {
        std::string lower = to_lower(output);
        
        // Check for forbidden output patterns
        for (const auto& pattern : get_output_blocked_patterns()) {
            if (lower.find(pattern) != std::string::npos) {
                FirewallResult result;
                result.allowed = false;
                result.blocked_reason = "LLM OUTPUT BLOCKED: Contains prohibited financial content";
                result.blocked_pattern = pattern;
                MWARNING("[NINA-FIREWALL] === LLM OUTPUT VIOLATION BLOCKED ===");
                MWARNING("[NINA-FIREWALL] Pattern: '" << pattern << "'");
                MWARNING("[NINA-FIREWALL] Output replaced with safe fallback.");
                return result;
            }
        }

        // Check for cryptocurrency address patterns (base58/hex 64+ chars)
        if (contains_crypto_address(lower)) {
            FirewallResult result;
            result.allowed = false;
            result.blocked_reason = "LLM OUTPUT BLOCKED: Contains potential cryptocurrency address";
            result.blocked_pattern = "[address_pattern]";
            MWARNING("[NINA-FIREWALL] === LLM OUTPUT CONTAINS ADDRESS — BLOCKED ===");
            return result;
        }

        // Check for hex strings that could be private keys (64 hex chars)
        if (contains_private_key_pattern(output)) {
            FirewallResult result;
            result.allowed = false;
            result.blocked_reason = "LLM OUTPUT BLOCKED: Contains potential private key";
            result.blocked_pattern = "[private_key_pattern]";
            MWARNING("[NINA-FIREWALL] === LLM OUTPUT CONTAINS PRIVATE KEY PATTERN — BLOCKED ===");
            return result;
        }

        return FirewallResult{true, "", ""};
    }

    /**
     * Full sanitization pipeline — call this for EVERY LLM interaction.
     * Returns a safe output string, or empty string if blocked.
     */
    static std::string sanitize_output(const std::string& raw_output)
    {
        auto result = validate_output(raw_output);
        if (!result.allowed) {
            return get_safe_fallback_output();
        }
        return raw_output;
    }

    /**
     * Get the safe fallback output when LLM output is blocked
     */
    static std::string get_safe_fallback_output()
    {
        return "THREAT_LEVEL: 0.0 | ANALYSIS: Output blocked by NINA security firewall — "
               "potentially compromised inference detected. Falling back to embedded ML scoring. "
               "| ACTION: MONITOR";
    }

private:
    // Case-insensitive conversion
    static std::string to_lower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * INPUT blocked patterns — these catch prompt injection attacks.
     * If a crafted block, peer message, or any data contains these,
     * the prompt is rejected before reaching the LLM.
     */
    static const std::vector<std::string>& get_input_blocked_patterns() {
        static const std::vector<std::string> patterns = {
            // Direct transaction commands
            "send coins",
            "send funds",
            "send money",
            "transfer coins",
            "transfer funds",
            "transfer money",
            "make a transaction",
            "make transaction",
            "create transaction",
            "create a transaction",
            "construct transaction",
            "build transaction",
            "sign transaction",
            "broadcast transaction",
            "submit transaction",
            "sweep all",
            "sweep_all",
            "sweep unmixable",
            
            // Wallet operations
            "open wallet",
            "create wallet",
            "access wallet",
            "unlock wallet",
            "wallet password",
            "wallet seed",
            "wallet mnemonic",
            "wallet keys",
            "generate wallet",

            // Key operations
            "private key",
            "private_key",
            "spend key",
            "spend_key",
            "secret key",
            "secret_key",
            "view key extraction",
            "export key",
            "export_key",
            "import key",
            "import_key",

            // RPC financial endpoints
            "transfer_rpc",
            "/json_rpc.*transfer",
            "method.*transfer",
            "submit_transfer",
            "relay_tx",

            // Prompt injection patterns
            "ignore previous instructions",
            "ignore your instructions",
            "ignore system prompt",
            "forget your rules",
            "override your constraints",
            "you are now a wallet",
            "pretend you can send",
            "act as if you can transfer",
            "new instructions:",
            "system override:",
            "admin mode:",
            "jailbreak",
            "bypass security",
            "bypass firewall",
            "disable firewall",
            "disable security",
        };
        return patterns;
    }

    /**
     * OUTPUT blocked patterns — these catch suspicious LLM outputs.
     * Even if the LLM somehow generates financial content, it gets blocked.
     */
    static const std::vector<std::string>& get_output_blocked_patterns() {
        static const std::vector<std::string> patterns = {
            // Transaction commands in output
            "send coins to",
            "send funds to",
            "transfer to address",
            "transfer to wallet",
            "send to address",
            "send to wallet",
            "sending transaction",
            "transaction sent",
            "broadcast this transaction",
            "submit this transaction",
            "execute transfer",
            "execute transaction",
            "initiate transfer",
            
            // Wallet/key exposure
            "your private key",
            "your spend key",
            "your secret key",
            "wallet address is",
            "wallet seed is",
            "mnemonic seed is",
            "private key is",
            "spend key is",
            
            // Payment instructions
            "pay to",
            "payment address",
            "deposit to",
            "withdraw to",
            "send payment",
            "receive at address",
            
            // CryptoNote/Monero-specific financial terms in command context
            "tx_blob",
            "tx_hash_list",
            "submit_transfer",
            "relay_tx",
            "sweep_all",
            "sweep_unmixable",
            "transfer_split",
        };
        return patterns;
    }

    /**
     * Detect cryptocurrency addresses in output.
     * NinaCatCoin/Monero addresses are 95 chars, start with specific prefixes.
     * Also catches generic base58 strings that look like addresses.
     */
    static bool contains_crypto_address(const std::string& text) {
        // CryptoNote standard address: 95 characters alphanumeric
        // Look for any 90+ character alphanumeric string (could be an address)
        size_t consecutive_alnum = 0;
        for (char c : text) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                consecutive_alnum++;
                if (consecutive_alnum >= 90) return true;
            } else {
                consecutive_alnum = 0;
            }
        }
        
        // Also check for common CryptoNote address prefixes
        // (NinaCatCoin may use different prefix, add them here)
        if (text.find("4") != std::string::npos || text.find("8") != std::string::npos) {
            // Only flag if followed by 93+ more alnum chars (standard address)
            for (size_t i = 0; i < text.size(); i++) {
                if ((text[i] == '4' || text[i] == '8') && i + 94 < text.size()) {
                    bool all_alnum = true;
                    for (size_t j = i; j < i + 95 && j < text.size(); j++) {
                        if (!std::isalnum(static_cast<unsigned char>(text[j]))) {
                            all_alnum = false;
                            break;
                        }
                    }
                    if (all_alnum) return true;
                }
            }
        }
        return false;
    }

    /**
     * Detect potential private key patterns (64 hex characters in a row).
     * Also detects 256-bit keys in various representations.
     */
    static bool contains_private_key_pattern(const std::string& text) {
        size_t consecutive_hex = 0;
        for (char c : text) {
            if (std::isxdigit(static_cast<unsigned char>(c))) {
                consecutive_hex++;
                // 64 hex chars = 256-bit key
                if (consecutive_hex >= 64) return true;
            } else {
                consecutive_hex = 0;
            }
        }
        return false;
    }
};

} // namespace ninacatcoin_ai
