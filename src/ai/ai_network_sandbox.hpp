// Copyright (c) 2026, The ninacatcoin Project
//
// Network sandbox - restricts IA network access

#pragma once

#include "ai_config.hpp"
#include <string>
#include <vector>
#include <set>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * @class NetworkSandbox
 * @brief Enforces strict network access control
 * 
 * CRITICAL SECURITY:
 * - Only allows P2P connections to ninacatcoin peers
 * - Blocks DNS resolution
 * - Blocks external connections
 * - Validates all network operations
 * - Logs all connection attempts
 */
class NetworkSandbox {
public:
    static NetworkSandbox& getInstance();

    // Initialize network sandbox
    bool initialize();

    // Check if connection is allowed
    bool isConnectionAllowed(
        const std::string& dest_ip,
        int dest_port,
        const std::string& protocol
    );

    // Check if peer IP is in network
    bool isPeerInNetwork(const std::string& ip);

    // Register valid peer
    void registerValidPeer(const std::string& peer_ip);

    // Get list of valid peers
    std::vector<std::string> getValidPeers();

    // Check if blacklisted
    bool isPeerBlacklisted(const std::string& ip);

    // Add to blacklist
    void addToBlacklist(const std::string& ip, const std::string& reason);

    // Verify sandbox active
    bool verifySandboxActive();

    // Get network audit log
    std::vector<std::string> getNetworkLog();

private:
    NetworkSandbox();
    ~NetworkSandbox();

    // Delete copy/move
    NetworkSandbox(const NetworkSandbox&) = delete;
    NetworkSandbox& operator=(const NetworkSandbox&) = delete;

    std::set<std::string> valid_peers;
    std::set<std::string> blacklisted_peers;
    std::vector<std::string> network_log;
    bool is_initialized;

    // Log connection attempt
    void logConnectionAttempt(
        const std::string& ip,
        int port,
        bool allowed
    );

    // Load peers from network
    bool loadValidPeers();

    // Validate peer connection
    bool validatePeerConnection(const std::string& ip, int port);
};

} // namespace ninacatcoin_ai
