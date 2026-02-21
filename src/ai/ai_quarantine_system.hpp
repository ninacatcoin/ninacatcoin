// Copyright (c) 2026, The ninacatcoin Project
//
// Quarantine system - isolates nodes with compromised IA code

#pragma once

#include "ai_config.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * @class QuarantineSystem
 * @brief Quarantines nodes that repeatedly fail validation
 * 
 * CRITICAL SECURITY:
 * - Blocks ALL network access
 * - Disables IA module completely
 * - Disables mining
 * - Adds node to global blacklist
 * - Notifies seed nodes
 * - Prevents further network participation
 * - Permanent until manual intervention
 */
class QuarantineSystem {
public:
    static QuarantineSystem& getInstance();

    // Initialize quarantine system
    bool initialize();

    /**
     * Implement quarantine on this node
     * Called when remediation fails 3 times
     */
    void implementQuarantine();

    /**
     * Check if node is quarantined
     */
    bool isQuarantined() const;

    /**
     * Get quarantine reason
     */
    std::string getQuarantineReason() const;

    /**
     * Get timestamp when quarantine was applied
     */
    uint64_t getQuarantineTimestamp() const;

private:
    QuarantineSystem();
    ~QuarantineSystem();

    // Delete copy/move
    QuarantineSystem(const QuarantineSystem&) = delete;
    QuarantineSystem& operator=(const QuarantineSystem&) = delete;

    bool quarantine_active;
    std::string quarantine_reason;
    uint64_t quarantine_timestamp;
    bool is_initialized;

    // Quarantine implementation steps

    /**
     * Set quarantine flags
     */
    void setQuarantineFlags();

    /**
     * Block all networking completely
     */
    void blockAllNetworking();

    /**
     * Notify seed nodes of quarantine
     */
    void notifySeedsOfQuarantine();

    /**
     * Add to global blacklist
     */
    void addToGlobalBlacklist();

    /**
     * Display critical message to user
     */
    void displayQuarantineMessage();

    /**
     * Close all active sockets
     */
    void closeAllSockets();

    /**
     * Disable firewall rules on this machine (windows)
     */
    void implementWindowsFirewallRules();

    /**
     * Implement Linux iptables rules (only ninacatcoin ports)
     */
    void implementLinuxIPTables();

    /**
     * Remove all quarantine firewall rules (cleanup)
     */
    void removeFirewallRules();

    /**
     * Write quarantine flag file
     */

    void writeQuarantineFlag();
};

/**
 * @class GlobalBlacklist
 * @brief Manages global blacklist of quarantined nodes
 */
class GlobalBlacklist {
public:
    static GlobalBlacklist& getInstance();

    // Sync blacklist from seed nodes
    bool syncFromSeedNodes();

    // Check if node is blacklisted
    bool isNodeBlacklisted(const std::string& node_ip);

    // Reject connection from blacklisted peer
    void rejectBlacklistedPeer(const std::string& peer_ip);

    // Add node to local blacklist
    void addToBlacklist(
        const std::string& node_ip,
        const std::string& reason
    );

    // Get blacklist size
    int getBlacklistSize() const;

private:
    GlobalBlacklist();
    ~GlobalBlacklist();

    GlobalBlacklist(const GlobalBlacklist&) = delete;
    GlobalBlacklist& operator=(const GlobalBlacklist&) = delete;

    std::vector<std::string> blacklisted_ips;
    bool is_initialized;

    // Load blacklist from local cache
    bool loadBlacklistFromCache();

    // Save blacklist to cache
    void saveBlacklistToCache();
};

} // namespace ninacatcoin_ai
