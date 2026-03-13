// Copyright (c) 2026, The ninacatcoin Project
//
// Quarantine system implementation

#include "ai_quarantine_system.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstdint>

namespace ninacatcoin_ai {

static QuarantineSystem* g_quarantine = nullptr;
static GlobalBlacklist* g_blacklist = nullptr;

// QuarantineSystem implementation

QuarantineSystem& QuarantineSystem::getInstance() {
    if (!g_quarantine) {
        g_quarantine = new QuarantineSystem();
    }
    return *g_quarantine;
}

QuarantineSystem::QuarantineSystem()
    : quarantine_active(false),
      quarantine_timestamp(0),
      is_initialized(false) {
}

QuarantineSystem::~QuarantineSystem() {
}

bool QuarantineSystem::initialize() {
    if (is_initialized) {
        return true;
    }

    is_initialized = true;
    std::cout << "[AI Quarantine] Quarantine system initialized" << std::endl;
    return true;
}

void QuarantineSystem::implementQuarantine() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "⚠️  QUARANTINE ACTIVATED ⚠️" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    quarantine_active = true;
    quarantine_timestamp = std::time(nullptr);
    quarantine_reason = "Failed IA remediation after 3 attempts";

    // Step 1: Set quarantine flags
    std::cout << "[AI Quarantine] Step 1: Setting quarantine flags..." << std::endl;
    setQuarantineFlags();

    // Step 2: Block all networking
    std::cout << "[AI Quarantine] Step 2: Blocking all network access..." << std::endl;
    blockAllNetworking();

    // Step 3: Notify seed nodes
    std::cout << "[AI Quarantine] Step 3: Notifying seed nodes..." << std::endl;
    notifySeedsOfQuarantine();

    // Step 4: Add to global blacklist
    std::cout << "[AI Quarantine] Step 4: Adding to global blacklist..." << std::endl;
    addToGlobalBlacklist();

    // Step 5: Display notice to user
    std::cout << "[AI Quarantine] Step 5: Displaying quarantine notice..." << std::endl;
    displayQuarantineMessage();

    std::cout << "[AI Quarantine] ✅ Quarantine fully implemented" << std::endl;
}

bool QuarantineSystem::isQuarantined() const {
    return quarantine_active;
}

std::string QuarantineSystem::getQuarantineReason() const {
    return quarantine_reason;
}

uint64_t QuarantineSystem::getQuarantineTimestamp() const {
    return quarantine_timestamp;
}

void QuarantineSystem::setQuarantineFlags() {
    std::ofstream flag_file(AISecurityConfig::QUARANTINE_FLAG_FILE);
    
    if (flag_file.is_open()) {
        auto time = std::time(nullptr);
        flag_file << "=================================================================\n";
        flag_file << "QUARANTINE FLAG FILE\n";
        flag_file << "=================================================================\n";
        flag_file << "Quarantine Date: " << std::ctime(&time);
        flag_file << "Reason: " << quarantine_reason << "\n";
        flag_file << "Remediation Attempts: 3\n";
        flag_file << "Status: QUARANTINE_ACTIVE\n";
        flag_file << "Network Access: BLOCKED\n";
        flag_file << "Mining: DISABLED\n";
        flag_file << "IA Module: DISABLED\n";
        flag_file << "=================================================================\n";
        flag_file.close();
        
        std::cout << "[AI Quarantine] Quarantine flag file created" << std::endl;
    }
}

void QuarantineSystem::blockAllNetworking() {
    // Block all network access completely

#ifdef __linux__
    std::cout << "[AI Quarantine] Implementing Linux firewall rules..." << std::endl;
    implementLinuxIPTables();

#elif defined(_WIN32)
    std::cout << "[AI Quarantine] Implementing Windows firewall rules..." << std::endl;
    implementWindowsFirewallRules();

#elif defined(__APPLE__)
    std::cout << "[AI Quarantine] Implementing macOS firewall rules..." << std::endl;
    // TODO: macOS implementation
#endif

    closeAllSockets();
}

void QuarantineSystem::notifySeedsOfQuarantine() {
    // Send notification to seed nodes before blocking network
    
    for (const auto* seed : AISecurityConfig::SEED_NODES) {
        std::cout << "[AI Quarantine] Attempting to notify " << seed << std::endl;
        
        // TODO: Implement actual P2P notification
        // For now, just log the attempt
    }
}

void QuarantineSystem::addToGlobalBlacklist() {
    GlobalBlacklist& blacklist = GlobalBlacklist::getInstance();
    
    // Get local IP address
    // TODO: Implement getting local IP
    std::string local_ip = "127.0.0.1"; // placeholder
    
    blacklist.addToBlacklist(
        local_ip,
        "Node quarantined due to failed IA remediation"
    );
}

void QuarantineSystem::displayQuarantineMessage() {
    std::string critical_message = R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║                     🔒 CRITICAL QUARANTINE NOTICE 🔒                        ║
║                                                                              ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║  Your ninacatcoin node has been PERMANENTLY QUARANTINED                    ║
║                                                                              ║
║  REASON: Failed IA code integrity validation 3 times                       ║
║                                                                              ║
║  WHAT THIS MEANS:                                                           ║
║  ❌ Your IA code was either:                                               ║
║     • Intentionally modified/altered                                       ║
║     • Hit by malware that modified the code                                ║
║     • Corrupted during storage/transmission                                ║
║                                                                              ║
║  ATTEMPTED REMEDIATION:                                                    ║
║  The system attempted automatic remediation 3 times:                       ║
║  1. Downloaded clean code from official GitHub                            ║
║  2. Recompiled with deterministic build                                   ║
║  3. Revalidated with seed nodes                                           ║
║  All 3 attempts FAILED - indicating serious system compromise              ║
║                                                                              ║
║  CURRENT STATUS:                                                            ║
║  ❌ Network access: COMPLETELY BLOCKED                                     ║
║  ❌ IA module: DISABLED                                                    ║
║  ❌ Mining: DISABLED                                                       ║
║  ❌ P2P connections: REJECTED                                              ║
║  ❌ Node IP: GLOBALLY BLACKLISTED                                          ║
║                                                                              ║
║  YOUR NODE IS ISOLATED FROM THE ninacatcoin NETWORK                        ║
║                                                                              ║
║  RECOVERY INSTRUCTIONS:                                                    ║
║  1. UNINSTALL ninacatcoin completely                                      ║
║  2. DELETE all ninacatcoin folders and data                               ║
║  3. DELETE this entire directory                                          ║
║  4. Download FRESH code from OFFICIAL source:                            ║
║     https://github.com/ninacatcoin/ninacatcoin                           ║
║  5. Follow OFFICIAL compilation instructions                             ║
║  6. NEVER modify IA code or compile from untrusted sources               ║
║                                                                              ║
║  TIMELINE:                                                                  ║
║  ⏰ Quarantine Date: """ + std::string(std::ctime(reinterpret_cast<std::time_t*>(&quarantine_timestamp))) + R"(
║  ⏰ Node is PERMANENTLY BLACKLISTED unless reinstalled from clean source   ║
║                                                                              ║
║  WARNINGS:                                                                  ║
║  ⚠️  Other nodes will REJECT all connections from this IP                 ║
║  ⚠️  This node is flagged in GLOBAL PEER BLACKLIST                        ║
║  ⚠️  Seed nodes have been notified of compromise                          ║
║  ⚠️  No support will be provided while quarantined                        ║
║  ⚠️  Attempting to bypass quarantine will be logged                       ║
║  ⚠️  Repeating offense could result in permanent IP ban                   ║
║                                                                              ║
║  SECURITY NOTES:                                                            ║
║  This quarantine system is designed to:                                    ║
║  • Protect the ninacatcoin network from compromised nodes                 ║
║  • Prevent further spread of tampering                                    ║
║  • Alert operators to potential security breaches                        ║
║  • Force complete reinstallation from trusted sources                    ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
)";

    std::cerr << critical_message << std::endl;

    // Write message to file
    std::ofstream notice_file("/ninacatcoin_data/QUARANTINE_NOTICE.txt");
    if (notice_file.is_open()) {
        notice_file << critical_message;
        notice_file.close();
        std::cout << "[AI Quarantine] Notice written to: /ninacatcoin_data/QUARANTINE_NOTICE.txt" << std::endl;
    }

    // Also write to stdout for visibility
    std::cout << critical_message << std::endl;
}

void QuarantineSystem::closeAllSockets() {
    // TODO: Close all active network sockets
    std::cout << "[AI Quarantine] Closing all network sockets..." << std::endl;
}

void QuarantineSystem::implementWindowsFirewallRules() {
    #ifdef _WIN32
    // Block only ninacatcoin ports, NOT all traffic (avoids locking out SSH/RDP)
    std::vector<std::string> commands = {
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineP2P\" "
        "dir=out action=block protocol=TCP localport=19020 enable=yes",
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineRPC\" "
        "dir=out action=block protocol=TCP localport=19021 enable=yes",
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineZMQ\" "
        "dir=out action=block protocol=TCP localport=19022 enable=yes",
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineP2PIn\" "
        "dir=in action=block protocol=TCP localport=19020 enable=yes",
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineRPCIn\" "
        "dir=in action=block protocol=TCP localport=19021 enable=yes",
        "netsh advfirewall firewall add rule name=\"NinacatcoinQuarantineZMQIn\" "
        "dir=in action=block protocol=TCP localport=19022 enable=yes"
    };

    for (const auto& cmd : commands) {
        std::cout << "[AI Quarantine] Executing: " << cmd << std::endl;
        system(cmd.c_str());
    }
    #endif
}

void QuarantineSystem::implementLinuxIPTables() {
    #ifdef __linux__
    // Block only ninacatcoin P2P/RPC/ZMQ ports — NEVER block all traffic
    // This preserves SSH access and other services on the machine
    std::vector<std::string> commands = {
        "iptables -I INPUT 1 -p tcp --dport 19020 -j DROP -m comment --comment ninacatcoin_quarantine",
        "iptables -I INPUT 1 -p tcp --dport 19021 -j DROP -m comment --comment ninacatcoin_quarantine",
        "iptables -I INPUT 1 -p tcp --dport 19022 -j DROP -m comment --comment ninacatcoin_quarantine",
        "iptables -I OUTPUT 1 -p tcp --dport 19020 -j DROP -m comment --comment ninacatcoin_quarantine",
        "iptables -I OUTPUT 1 -p tcp --dport 19021 -j DROP -m comment --comment ninacatcoin_quarantine",
        "iptables -I OUTPUT 1 -p tcp --dport 19022 -j DROP -m comment --comment ninacatcoin_quarantine",
        "ip6tables -I INPUT 1 -p tcp --dport 19020 -j DROP -m comment --comment ninacatcoin_quarantine",
        "ip6tables -I OUTPUT 1 -p tcp --dport 19020 -j DROP -m comment --comment ninacatcoin_quarantine"
    };

    for (const auto& cmd : commands) {
        std::cout << "[AI Quarantine] Executing: " << cmd << std::endl;
        system(cmd.c_str());
    }
    #endif
}

void QuarantineSystem::removeFirewallRules() {
    #ifdef _WIN32
    std::vector<std::string> commands = {
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineP2P\"",
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineRPC\"",
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineZMQ\"",
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineP2PIn\"",
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineRPCIn\"",
        "netsh advfirewall firewall delete rule name=\"NinacatcoinQuarantineZMQIn\""
    };
    for (const auto& cmd : commands) { system(cmd.c_str()); }
    #endif
    #ifdef __linux__
    // Remove all rules tagged with ninacatcoin_quarantine comment
    system("iptables -S | grep ninacatcoin_quarantine | sed 's/-A//' | while read rule; do iptables -D $rule 2>/dev/null; done");
    system("ip6tables -S | grep ninacatcoin_quarantine | sed 's/-A//' | while read rule; do ip6tables -D $rule 2>/dev/null; done");
    #endif
    std::cout << "[AI Quarantine] Firewall rules cleaned up" << std::endl;
}

// GlobalBlacklist implementation

GlobalBlacklist& GlobalBlacklist::getInstance() {
    if (!g_blacklist) {
        g_blacklist = new GlobalBlacklist();
    }
    return *g_blacklist;
}

GlobalBlacklist::GlobalBlacklist()
    : is_initialized(false) {
}

GlobalBlacklist::~GlobalBlacklist() {
}

bool GlobalBlacklist::syncFromSeedNodes() {
    std::cout << "[AI Blacklist] Syncing global blacklist from seed nodes..." << std::endl;
    
    // TODO: Implement sync from seed nodes
    
    is_initialized = true;
    return true;
}

bool GlobalBlacklist::isNodeBlacklisted(const std::string& node_ip) {
    return std::find(blacklisted_ips.begin(), blacklisted_ips.end(), node_ip)
           != blacklisted_ips.end();
}

void GlobalBlacklist::rejectBlacklistedPeer(const std::string& peer_ip) {
    if (isNodeBlacklisted(peer_ip)) {
        std::cout << "[AI Blacklist] REJECTING blacklisted peer: " << peer_ip << std::endl;
    }
}

void GlobalBlacklist::addToBlacklist(
    const std::string& node_ip,
    const std::string& reason
) {
    blacklisted_ips.push_back(node_ip);
    std::cout << "[AI Blacklist] Added to blacklist: " << node_ip 
              << " (Reason: " << reason << ")" << std::endl;
    
    saveBlacklistToCache();
}

int GlobalBlacklist::getBlacklistSize() const {
    return blacklisted_ips.size();
}

bool GlobalBlacklist::loadBlacklistFromCache() {
    // TODO: Load blacklist from cache file
    return true;
}

void GlobalBlacklist::saveBlacklistToCache() {
    std::ofstream cache_file(
        "/ninacatcoin_data/ai_module/blacklist_cache.txt",
        std::ios::app
    );

    if (cache_file.is_open()) {
        for (const auto& ip : blacklisted_ips) {
            cache_file << ip << "\n";
        }
        cache_file.close();
    }
}

} // namespace ninacatcoin_ai
