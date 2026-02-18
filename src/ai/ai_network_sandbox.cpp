// Copyright (c) 2026, The ninacatcoin Project
//
// Network sandbox implementation

#include "ai_network_sandbox.hpp"
#include <iostream>

namespace ninacatcoin_ai {

static NetworkSandbox* g_net_sandbox = nullptr;

NetworkSandbox& NetworkSandbox::getInstance() {
    if (!g_net_sandbox) {
        g_net_sandbox = new NetworkSandbox();
    }
    return *g_net_sandbox;
}

NetworkSandbox::NetworkSandbox()
    : is_initialized(false) {
}

NetworkSandbox::~NetworkSandbox() {
}

bool NetworkSandbox::initialize() {
    if (is_initialized) {
        return true;
    }

    if (!loadValidPeers()) {
        std::cerr << "[AI Network] Warning: Could not load valid peers" << std::endl;
    }

    is_initialized = true;
    std::cout << "[AI Network] Network sandbox initialized" << std::endl;
    return true;
}

bool NetworkSandbox::isConnectionAllowed(
    const std::string& dest_ip,
    int dest_port,
    const std::string& protocol
) {
    if (!is_initialized) {
        return false;
    }

    // ONLY allow P2P protocol
    if (protocol != "NINACATCOIN_P2P" && protocol != "levin") {
        logConnectionAttempt(dest_ip, dest_port, false);
        std::cerr << "[AI Network] DENIED: Non-P2P protocol: " << protocol << std::endl;
        return false;
    }

    // ONLY allow P2P ports
    if (dest_port < AISecurityConfig::P2P_PORT_MIN ||
        dest_port > AISecurityConfig::P2P_PORT_MAX) {
        logConnectionAttempt(dest_ip, dest_port, false);
        std::cerr << "[AI Network] DENIED: Invalid port: " << dest_port << std::endl;
        return false;
    }

    // Check if peer is in valid network
    if (!isPeerInNetwork(dest_ip)) {
        logConnectionAttempt(dest_ip, dest_port, false);
        std::cerr << "[AI Network] DENIED: Peer not in network: " << dest_ip << std::endl;
        return false;
    }

    // Check if peer is blacklisted
    if (isPeerBlacklisted(dest_ip)) {
        logConnectionAttempt(dest_ip, dest_port, false);
        std::cerr << "[AI Network] DENIED: Peer is blacklisted: " << dest_ip << std::endl;
        return false;
    }

    logConnectionAttempt(dest_ip, dest_port, true);
    return true;
}

bool NetworkSandbox::isPeerInNetwork(const std::string& ip) {
    return valid_peers.find(ip) != valid_peers.end();
}

void NetworkSandbox::registerValidPeer(const std::string& peer_ip) {
    valid_peers.insert(peer_ip);
    std::cout << "[AI Network] Registered peer: " << peer_ip << std::endl;
}

std::vector<std::string> NetworkSandbox::getValidPeers() {
    std::vector<std::string> peers(valid_peers.begin(), valid_peers.end());
    return peers;
}

bool NetworkSandbox::isPeerBlacklisted(const std::string& ip) {
    return blacklisted_peers.find(ip) != blacklisted_peers.end();
}

void NetworkSandbox::addToBlacklist(
    const std::string& ip,
    const std::string& reason
) {
    blacklisted_peers.insert(ip);
    std::cout << "[AI Network] Blacklisted peer: " << ip 
              << " (Reason: " << reason << ")" << std::endl;
}

bool NetworkSandbox::verifySandboxActive() {
    return is_initialized;
}

std::vector<std::string> NetworkSandbox::getNetworkLog() {
    return network_log;
}

void NetworkSandbox::logConnectionAttempt(
    const std::string& ip,
    int port,
    bool allowed
) {
    std::string log_entry = 
        "[" + std::string(allowed ? "ALLOW" : "DENY") + "] " +
        ip + ":" + std::to_string(port);
    
    network_log.push_back(log_entry);
    
    if (!allowed && network_log.size() % 10 == 0) {
        // Log suspicious activity
        std::cerr << "[AI Network] Suspicious activity detected" << std::endl;
    }
}

bool NetworkSandbox::loadValidPeers() {
    // TODO: Load valid peers from seed nodes or configuration
    // For now, just return true
    return true;
}

bool NetworkSandbox::validatePeerConnection(const std::string& ip, int port) {
    // Verify peer credentials and connection validity
    // TODO: Implement peer validation
    return true;
}

} // namespace ninacatcoin_ai
