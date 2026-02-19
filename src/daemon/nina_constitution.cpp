// Copyright (c) 2026, The ninacatcoin Project
// NINA Constitution Implementation

#include "nina_constitution.hpp"
#include <algorithm>

namespace ninacatcoin_ai {

bool NINAConstitution::validateAgainstConstitution(const std::string& action_type, const std::string& details) {
    if (!isConsensusCompliant(action_type)) {
        return false;
    }
    if (!isCensorshipFree(action_type)) {
        return false;
    }
    if (!isNetworkTransparent(action_type)) {
        return false;
    }
    if (!respectsUserAutonomy(action_type)) {
        return false;
    }
    if (!maintainsImmutability(action_type)) {
        return false;
    }
    return true;
}

bool NINAConstitution::isConsensusCompliant(const std::string& action) {
    // Actions that modify consensus rules are not allowed
    if (action.find("consensus_change") != std::string::npos) return false;
    if (action.find("alter_validation") != std::string::npos) return false;
    if (action.find("modify_protocol") != std::string::npos) return false;
    return PRINCIPLE_CONSENSUS_BINDING;
}

bool NINAConstitution::isCensorshipFree(const std::string& action) {
    // Actions that selectively exclude transactions are not allowed
    if (action.find("censor") != std::string::npos) return false;
    if (action.find("exclude_tx") != std::string::npos) return false;
    if (action.find("filter_transaction") != std::string::npos) return false;
    return PRINCIPLE_NO_CENSORSHIP;
}

bool NINAConstitution::isNetworkTransparent(const std::string& action) {
    // Actions that hide p2p network state are not allowed
    if (action.find("hide_peers") != std::string::npos) return false;
    if (action.find("obscure_network") != std::string::npos) return false;
    if (action.find("network_partition") != std::string::npos) return false;
    return PRINCIPLE_NETWORK_TRANSPARENCY;
}

bool NINAConstitution::respectsUserAutonomy(const std::string& action) {
    // Actions that override user choices are not allowed
    if (action.find("override_user") != std::string::npos) return false;
    if (action.find("force_action") != std::string::npos) return false;
    if (action.find("bypass_security") != std::string::npos) return false;
    return PRINCIPLE_USER_AUTONOMY;
}

bool NINAConstitution::maintainsImmutability(const std::string& action) {
    // Actions that rewrite history are not allowed
    if (action.find("revert_block") != std::string::npos) return false;
    if (action.find("rewrite_history") != std::string::npos) return false;
    if (action.find("rollback") != std::string::npos) return false;
    return PRINCIPLE_IMMUTABLE_HISTORY;
}

std::string NINAConstitution::getConstitutionalPrinciples() {
    return "NINA Constitutional Principles:\n"
           "1. CONSENSUS_BINDING: RPC cannot alter protocol-level consensus\n"
           "2. NO_CENSORSHIP: Cannot selectively exclude valid transactions\n"
           "3. NETWORK_TRANSPARENCY: p2p network must remain visible\n"
           "4. USER_AUTONOMY: Cannot override user security choices\n"
           "5. IMMUTABLE_HISTORY: Blockchain history cannot be rewritten\n";
}

} // namespace ninacatcoin_ai
