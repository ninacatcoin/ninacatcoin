// Copyright (c) 2026, The ninacatcoin Project
// NINA Constitution Header - Defines immutable governance constraints

#pragma once

#include <string>
#include <vector>

namespace ninacatcoin_ai {

/**
 * NINAConstitution defines immutable governance rules that cannot be violated
 * by any NINA AI suggestion, regardless of confidence score or urgency.
 */
class NINAConstitution {
public:
    // Core Constitutional Principles
    static constexpr bool PRINCIPLE_CONSENSUS_BINDING = true;      // RPC must not alter consensus at protocol level
    static constexpr bool PRINCIPLE_NO_CENSORSHIP = true;          // Cannot selectively exclude valid transactions
    static constexpr bool PRINCIPLE_NETWORK_TRANSPARENCY = true;   // Must maintain p2p network visibility
    static constexpr bool PRINCIPLE_USER_AUTONOMY = true;          // Cannot override user security choices
    static constexpr bool PRINCIPLE_IMMUTABLE_HISTORY = true;      // Blockchain history cannot be rewritten
    
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
